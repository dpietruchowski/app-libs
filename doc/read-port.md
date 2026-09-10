# The read port

`DbRepository` is one table, one entity, reads and writes together. A view that needs
data from several tables has no shape to ask for, so the columns get joined onto the
write entity and stay there — the entity ends up carrying aggregates that belong to
other owners, kept alive by nothing but the read path.

The read port is the second door. It answers **one SELECT → one typed row**, has no
write methods at all, and separates two decisions the toolkit used to bundle:

| Decision | Who makes it |
|---|---|
| `FROM`, `JOIN`, `WHERE`, `ORDER BY`, `LIMIT` | the shape — a `Select` the caller builds |
| which columns come back, and how they are read | the projection |

Because those are separate, one shape serves a wide row, a narrow row and a count.

## The pieces

| Class | Header | Role |
|---|---|---|
| `Expr` | `dbtoolkit/query/expr.h` | one SQL fragment plus the name its value takes in the row |
| `Projection` | `dbtoolkit/query/projection.h` | a named group of expressions: what enters `SELECT`, and how to read it back |
| `ProjectedRow` | `dbtoolkit/query/projection.h` | one result row split into those groups |
| `DbProjection<Row>` | `dbtoolkit/dbprojection.h` | runs a shape through a projection and maps rows to `Row` |

`Select::projected()` and `Select::clearProjected()` are the seam: expressions given to
`projected()` are written verbatim, without the `FROM` alias that plain columns receive.

### Expr

A fragment evaluates to one value per row. It is not a query and never goes to the
database alone — it is pasted into `SELECT`, `WHERE` and `ORDER BY`.

```cpp
Expr(TableAlias("atl"), "success_rate")       // toSql() "atl.success_rate", key() "success_rate"
Expr(someCase, "accuracy")                    // toSql() from any SqlQuery, key() "accuracy"
Expr("COUNT(*)", "total")                     // raw fragment plus its name
```

`Expr` derives from `SqlQuery`, so it goes anywhere a `SqlQuery` is accepted — including
into `Case` — and expressions nest.

### Projection

```cpp
Projection(TableAlias("atl"), { "total_attempts", "success_rate" });
Projection("calc", { Expr(branches, "accuracy") });
```

The first form covers plain columns from one alias; the group name is the alias. The
second takes any expressions under a group name of its own.

One object serves both directions, which is the point: the column list and the way it is
read back can no longer drift apart.

```cpp
projection.selectExpressions();
// { "atl.total_attempts AS atl_total_attempts", "atl.success_rate AS atl_success_rate" }

projection.extract(row);
// { "total_attempts": 6, "success_rate": 0.3 }
```

The output name is `<group>_<key>`; extraction strips the prefix, so the mapper receives
the original column names. Two aliases over the same table stay apart on their prefixes.

### DbProjection

```cpp
DbProjection<Row> rows(storage, projections, &rowFrom);

rows.findAll(shape);     // QVector<Row>
rows.findFirst(shape);   // std::optional<Row>
rows.count(shape);       // int
rows.exists(shape);      // bool
```

`projected(shape)` and `counting(shape)` return the `Select` those calls would run, for
inspection without executing.

There is no `save`, `insert`, `upsert` or `remove`. That is the design, not an omission:
a projection is a snapshot with no identity and no owner.

## Behaviour

| Call | Contract |
|---|---|
| `findAll` | replaces **only** the columns; joins, `WHERE`, `ORDER BY` and `LIMIT` of the shape are untouched |
| `findFirst` | sets `LIMIT 1`, **overriding** any limit on the shape; without an `ORDER BY` the row returned is undefined |
| `count` | **honours `LIMIT`**, **drops `ORDER BY`**, projects the constant `1` instead of columns |
| `exists` | `count(shape) > 0` |

`count` honours the limit because a shape the caller deliberately capped should not change
meaning on its way to being counted. It drops the ordering because sorting a subquery whose
rows are only tallied is pure cost.

## Sorting and filtering on a computed value

This is what the separation buys. A value derived from several joined tables can order the
result before `LIMIT` cuts it, whether or not that value is returned.

```cpp
Case branches;
branches.when(Where(kSchedule, "fact_id").isNotNull(), accuracyOf(kReview))
        .otherwise(accuracyOf(kLearning));
const Expr accuracy(branches, "accuracy");

Select shape = ...;                                  // three joins
shape.orderBy(accuracy.toSql() + " DESC");
shape.where(accuracy.toSql() + " >= 0.9");
```

`Case` accepts a condition and an expression — `when(const Where&, const SqlQuery&)` — so a
searched `CASE` is built like every other statement rather than glued together as a string.
The subject form, `Case("column").when(1, "pl")`, is unchanged and still quotes its values.

### Demonstrated

Five rows, where the stored `success_rate` column deliberately contradicts the counters, so
a computed accuracy and a stored one cannot be confused:

| fact | learning | review | schedule | stored rate | computed accuracy |
|---|---|---|---|---|---|
| f1 | 9/10 | — | no | 0.10 | 0.900 |
| f2 | 1/4 | — | no | 0.99 | 0.250 |
| f3 | 2/2 | — | no | 0.20 | 1.000 |
| f4 | 3/6 | 4/4 | yes | 0.30 | 1.000 |
| f5 | — | — | no | — | 0.000 |

- Ordering by the computed accuracy gives `f3, f4, f1, f2, f5`; ordering by the stored
  column gives `f2, f4, f3, f1, f5`. The expression is genuinely evaluated.
- `f4` scores 1.000, not 0.500: it has a schedule, so the `CASE` takes the review branch.
  No amount of post-processing in C++ recovers that choice from a plain join.
- `f5` has no summary row at all and still appears, at 0.000 — the `LEFT JOIN` keeps it and
  `COALESCE` gives the missing counters a value.
- With `LIMIT 2` the result is `f3, f4` — the two genuinely highest, because the database
  ordered all five before cutting.
- **With the statistics dropped from the projection the order is identical.** The database
  computed the value, sorted on it, and discarded it without sending a single number.

That last point is the whole reason the port exists: an entity can stop carrying another
aggregate's fields and a list still gets the right rows in the right order, in one query.

## Rules

**Order and filter on the expression, never on the output alias.** SQLite accepts
`calc_accuracy` in `ORDER BY` but not in `WHERE`, and the alias exists only while the
expression is projected. `Expr::toSql()` is correct in all three positions. The cost is that
the fragment appears more than once in the statement; it is evaluated per row either way.

**Sorting before a limit is correctness, not performance.** A shape that limits without
ordering returns an arbitrary subset, and sorting that subset afterwards in C++ orders the
wrong set. The port cannot introduce this fault — it never touches `ORDER BY` or `LIMIT` —
but it cannot repair a shape that already has it.

**An aliased join needs a natural key on the far side.** If a join matches more than one row
on the right, rows multiply. `LIMIT 3` then returns three rows but fewer than three distinct
entities, silently. Nothing in the type system catches this; only the schema does. Add the
unique index before adding the second aliased join.

**An undeclared group yields an empty map and a warning.** `ProjectedRow::of("nope")` logs
and returns nothing rather than throwing, matching how the rest of the toolkit degrades. A
mistyped group name therefore produces default-constructed fields, not a crash.

## Migrating a repository

`Join::withColumns()` and `withPrefix()` still work, so nothing has to change at once. A
shape that already carries join columns is normalised by the projection — `projected()`
clears both column lists before writing its own — so the entity path and the row path can
run against one shape while callers move over.

The end state is a join that is only a predicate:

```cpp
Join("attempt_summaries")
    .as(alias)
    .on(kProgress, "fact_id")
    .equals("fact_id")
    .andColumn("stage")
    .equalsValue(stage);
```

with the columns declared by a `Projection` instead.
