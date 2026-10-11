# Diagram — text to SVG

Status: revision 4. Steps 1–4 of the plan are implemented; this text describes the code as built.

## Goal

A course pasted from a chat must be able to carry its own diagrams. The chat writes a short
text description, and the app lays it out and draws it on the device, offline. The first
supported kind is the sequence diagram.

The syntax is a subset of PlantUML. A model already writes it without extra instructions, and
the author can preview the same text in real PlantUML on the desktop. **Only the syntax is
shared. No PlantUML code is used, ported or consulted.** The parser, the layout and the drawing
are our own implementation.

## Constraints

- Offline and deterministic: the same text, width and style always produce the same SVG.
- Responsive: the available width is an input to the layout, not a scale factor applied
  afterwards. Text keeps its size, and the diagram is laid out again when the width changes.
- Themeable without a re-layout: geometry carries colour *roles*, and the palette is applied
  only when the SVG is written.
- Data only: the pasted text describes a diagram and can never execute anything.
- `libs` is shared with another app, so everything is additive: a new directory and a new
  target, and nothing existing changes.
- Rendering stays cheap. The SVG uses only `rect`, `line`, `polyline`, `polygon` and `text`, with no
  filters, gradients, masks or CSS, so it stays within what Qt Svg (SVG Tiny 1.2) draws.

## Pipeline

```
text ──parse──▶ SequenceDiagram ──layout(width, style, measurer)──▶ Scene ──write(palette, pixelRatio)──▶ SVG
```

| Stage | Input → output | Depends on |
|---|---|---|
| `SequenceParser` | `QString` → `Result<SequenceDiagram>` | Qt Core, `app_async` (`Result`) |
| `SequenceLayout` | diagram + `DiagramStyle` + width + `TextMeasurer` → `Scene` | Qt Core, Qt Gui (`DiagramStyle::font()`) |
| `SvgSceneWriter` | `Scene` + `DiagramPalette` + pixel ratio → `QString` | Qt Core, Qt Gui (`QColor`) |
| `FontTextMeasurer` | implements `TextMeasurer` with `QFontMetricsF` | Qt Gui |

The library does not link `Qt6::Svg`. The writer only builds a string, and its tests check the
output with `QXmlStreamReader`. The only test that uses `QSvgRenderer` is a smoke test.

`Scene` is the seam because it lets layout and writing be tested separately, and because a theme
change only re-runs the writer. It can also later serve an activity layout or a QML emitter.
Those are possibilities only: no field is added to `Scene` for them now.

## Directory and target

```
libs/cpp/diagram/            target app_diagram (STATIC; PUBLIC Qt6::Core, Qt6::Gui, app_async)
    sequencediagram.h        model: participants, steps, warnings
    sequenceparser.h/.cpp
    scene.h                  geometry primitives + colour roles
    textmeasurer.h/.cpp      interface, plus FontTextMeasurer
    diagramstyle.h           sizes, gaps, font family and size (the only source of both)
    diagrampalette.h         role → QColor
    sequencelayout.h/.cpp
    svgscenewriter.h/.cpp
    diagramsvgprovider.h/.cpp  QObject for QML: text + width + colours → data: URL + image size
libs/tests/diagram/          diagram_tests (GoogleTest), FakeTextMeasurer
```

The directory's own `CMakeLists.txt` calls `find_package(Qt6 REQUIRED COMPONENTS Core Gui)`, so
it does not rely on Gui arriving through Quick. `libs/CMakeLists.txt` gets
`add_subdirectory(cpp/diagram)`, and `libs/tests/CMakeLists.txt` gets `add_subdirectory(diagram)`.

## Syntax, version 1

Input comes from a chat, so the parser is lenient about decoration and strict about structure.
Every line falls into one of three classes.

**Understood:**

```
@startuml / @enduml                          optional
title Login handshake
participant Client
participant "Auth server" as Auth
actor User                                   a participant drawn with a figure
database DB                                  also entity, boundary, control, collections,
                                             queue: all drawn as a plain box
Client -> Auth : credentials                 solid line, filled head
Auth --> Client : token                      dashed line (reply)
Client ->> Auth : event                      open head (async); also -->>
Auth <- Client : x                           reversed arrows <- and <--
Client -> Client : cache token               self message
note over Client, Auth : TLS                 also: note left of X / note right of X
note right : text                            attached to the right end of the previous message
                                             (left: its left end, over: its whole span)
note left of X                               multi-line note body, closed by `end note`
...
end note
alt valid                                    groups: alt, opt, loop, par, break, critical,
...                                          group; `else` in any of them; `end` or
else invalid                                 `end alt`; nestable
...
end
== Later ==                                  section divider
... 5 minutes later ...                      delay, drawn as a divider (also a bare `...`)
' comment, /' block comment '/
```

`return label` is a dashed reply to the innermost open call. Every solid message between two
different participants opens a call; `return` closes the innermost one, and so does a dashed
message that goes back along it. A later declaration of a participant updates its label and kind.

**Ignored, with a warning:** `skinparam` (also as a `{…}` block), `hide`, `show`, `!theme` and
other `!` directives, `autonumber`, `activate`, `deactivate`, `destroy`, `create`,
`autoactivate`, `scale`, `header`, `footer`, `caption`, `newpage`, `mainframe`, `box`, spacers
`|||`, `legend … endlegend`, `<style> … </style>`, stereotypes `<<x>>`, colour suffixes such as
`#red`, arrow colours `-[#red]->`, and activation suffixes `++ -- ** !!`. They change how
PlantUML draws the diagram, not what it means. Warnings are collected in
`SequenceDiagram::warnings` so the import preview can show them.

**Error:** anything else. Examples are an arrow without a target, `end` or `else` without an
open group, a group, note or block left unclosed at the end, `return` with no open call, or
`note right : …` with no message before it. Parsing stops at the first error, reported as
`Line N: …`. `Result<T>` carries one string, and one precise error is enough for the user to
fix the text in the chat. This is a deliberate choice. Common mistakes get a hint in the
message: a two-way `<->`, a gate `[->`, `ref over`, and an unquoted name with spaces. A diagram
without participants is the one error without a line number. Input longer than 20 000
characters is rejected before parsing, and a diagram with more than 300 steps is rejected at the
line that crosses the limit, so pasted text can never produce an image too tall to rasterize.

- A participant that is not declared is created when it is first used. Keywords can still be
  used as participant names in messages, because a message is recognised before a statement.
- `\n` in a label is a line break. A byte-order mark is dropped and typographic quotes `“ ” „`
  read as `"`.

## Model

```cpp
struct Participant { QString id; QString label; ParticipantKind kind; };
struct Message { int from; int to; QString label; LineStyle line; MessageHead head; };
struct Note { int first; int last; NotePlacement placement; QString text; };
struct Divider { QString label; };
struct GroupStart { QString keyword; QString label; };
struct GroupElse { QString label; };
struct GroupEnd {};
using SequenceStep = std::variant<Message, Note, Divider, GroupStart, GroupElse, GroupEnd>;
struct SequenceDiagram {
    QString title;
    QList<Participant> participants;
    QList<SequenceStep> steps;
    QStringList warnings;
};
```

Participants are referenced by index, resolved while parsing, and a note always has
`first <= last`. Groups are a flat sequence of start, else and end markers that the parser has
already checked for balance, so the layout keeps a simple stack. The group keyword is kept as
lowercase text, since the layout only prints it in the frame's tab. `MessageHead` (model) and
`ArrowHead` (scene) are separate enums, so the scene does not depend on the model.

## Scene

```cpp
enum class ColorRole { Background, Line, Text, MutedText, BoxFill, BoxStroke, NoteFill, NoteStroke, GroupStroke, Divider };
enum class ArrowHead { None, Filled, Open };
struct SceneRect { QRectF rect; qreal radius; std::optional<ColorRole> fill; std::optional<ColorRole> stroke; bool dashed; };
struct SceneLine { QList<QPointF> points; ColorRole color; bool dashed; ArrowHead head; };
struct SceneText { QRectF bounds; Qt::Alignment align; QStringList lines; ColorRole color; };
using SceneItem = std::variant<SceneRect, SceneLine, SceneText>;
struct Scene {
    QSizeF size;
    QString fontFamily; qreal fontPixelSize; qreal lineHeight; qreal ascent; qreal arrowHeadSize;
    QList<SceneItem> items;
};
```

The layout has already wrapped text into lines and computed its `bounds`, so the writer never
measures anything and tests can check that text does not overlap. The scene carries the font
and the measurer's `lineHeight` and `ascent`, so the writer places each baseline exactly where
the layout measured it.

`items` is one list in drawing order: lifelines, then group frames, then everything else in step
order. One ordered list is needed because a label's `Background` rectangle has to come after the
lines it covers and right before its own text. Message, self-message and group-condition labels
get such a rectangle, `style.labelHalo` wider than the text on each side, so a lifeline they
cross does not strike through them. A message label ends `arrowHeadSize / 2` above its arrow,
clear of the arrow head's wings. The scene size is rounded up to whole pixels, so the SVG's
`width`, `height` and `viewBox` are integers.

The writer drops characters that XML 1.0 forbids (control characters other than tab and line
breaks, U+FFFE, U+FFFF), so text pasted from anywhere still yields a well-formed document. A
filled arrow head is a polygon without a stroke, so its tip ends exactly on the lifeline. A `SceneLine` with
more than two points is a polyline (self loops, the actor figure); its head sits on the last
segment.

## Layout (sequence): from the width inwards

The available width is fixed first, and content wraps to fit it. Wrapping is the only response
to a lack of space.

1. **Columns.** With N participants there are N columns, one lifeline in the middle of each.
   A column's minimum is what its participant needs so that no word of its name is broken: the
   widest word plus padding and `style.columnGap`, never less than `style.minColumnWidth` and
   never more than `style.minLabelWidth`. Columns are as equal as possible: the available width
   is shared out evenly, and a column whose minimum is larger than its share keeps the minimum
   while the others split the rest.
2. **Labels.** Every label sits on its own row, so it may cross lifelines; its background
   rectangle keeps it readable.
   - A participant label wraps to its column.
   - A message label wraps to the readable width: the distance between its lifelines, but at
     least `style.minLabelWidth` and half the content width, and at most the content width. It
     is centred on its arrow and shifted back inside the scene when it would stick out.
   - A self-message loop and its label go to the side with more room, the right unless the
     right has less than `style.minLabelWidth` and the left has more. The label wraps to that
     room, so on the last column it turns left instead of reserving space.
   - A note `over` wraps to the span it covers, widened to the readable width and centred on the
     span. A note `left of` / `right of` starts beside its lifeline and may reach past the
     neighbour; when the room to the scene edge is narrower than `style.minLabelWidth` and the
     text would have to wrap there, it falls back to `over` its own lifeline.
   - Wrapping breaks at spaces first. A token that is still too long is broken by character,
     so a name like `OAuthAuthorizationServer` never overflows. Blank labels draw nothing.
   - Each measured width gets the `style.textSafety` margin of about 6%. That absorbs small
     differences between `QFontMetricsF` and the SVG renderer. The font size is an integer, so
     the measurer and the writer use the same size.
3. **Rows.** Rows go top to bottom. Each step advances `y` by its own height: the wrapped label
   height plus the arrow, the note box, the divider band, or the group header. A group frame
   always spans the full width of the scene, inset by `style.groupInset` for each level of
   nesting, up to `style.maxGroupIndent` levels and never so deep that the frame gets narrower
   than half of `style.minLabelWidth`. Its condition sits beside the keyword tab, or below it
   when the room beside the tab is too narrow. The
   frame does not depend on its contents, so empty groups and groups holding only notes need no
   special case, and the layout never has to go back once it reaches `end`.
4. **Feet.** Participant boxes are drawn at the top, aligned to the bottom of the row, and
   repeated at the bottom, aligned to its top, when there are more than
   `style.repeatFootAfterSteps` steps. Lifelines are dashed vertical lines between the rows.
5. **When it does not fit.** If the column minimums add up to more than the available width
   (long single-word names, or more than about five participants on a phone), the scene is laid
   out at those minimums and comes out wider than the viewport. The integration then gives the image
   an explicit display `width` equal to the viewport, so the image is scaled down. Rich text
   cannot scroll an image sideways, so scaling is the only fallback here. It is the one case
   where text gets smaller, and it is documented as such. The scale is available width ÷ scene
   width. If it falls below `0.75` at the reference width of 360, the import preview warns the
   author, so they can shorten the diagram in the chat before saving.

Invariants, each checked by tests:

- every primitive lies inside `scene.size`;
- the `scene.size` width is at most the available width, except in the case covered by step 5;
- no two text `bounds` overlap;
- every message line starts and ends on the x positions of its lifelines;
- `y` is strictly increasing across steps;
- group frames nest properly, so an inner frame lies inside its outer frame.

## Sharpness on a high-DPR screen

Phones have a device pixel ratio of about 2.6 to 3. The writer emits the root element as
`<svg width="W·r" height="H·r" viewBox="0 0 W H">`, where `r` is the same `sourcePixelRatio`
(3) that the bundled PNGs use.

The `<img>` tag gets the logical size (W × H) in its own `width` and `height` attributes, so the
image takes its logical place in the text whatever its natural size.

**Hypothesis, to be confirmed by the spike:** a natural size of W·r makes the rich-text image
rasterize at physical resolution, so its text is sharp. `QQuickText` may rasterize to the
displayed width instead. If it does, the result is no worse than the existing icons, which
already go through the same path at their logical size.

## Testing

- **Parser:** each understood form; implicit participants; aliases; `\n`; comments; that each
  ignored directive is recorded in `warnings`; and for each error kind, the error together with
  its line number. Unbalanced and nested groups are covered too.
- **Layout:** uses `FakeTextMeasurer`, where width is the character count times a constant, so
  geometry can be asserted exactly. Covers the invariants at widths of 120 to 720. Also checks
  that a narrower width produces more lines rather than smaller text, that labels wrap at words
  and may outgrow their arrow, that side notes sit beside their lifeline or fall back to `over`,
  that self messages turn to the roomier side, that long tokens are broken by character, that
  deep nesting stays inside the scene, and that too many participants take the overflow path.
- **Writer:** golden SVG strings for small scenes. Also checks, with `QXmlStreamReader`, that the
  output is well-formed XML using only the allowed elements and attributes.
- **Smoke test:** `QSvgRenderer` loads the writer's output, checks its size and viewBox, and
  paints it into a `QImage`: the background colour is where it belongs and a reasonable share of
  pixels carries ink. `isValid()` alone only proves the XML parses, because Qt Svg skips unknown
  elements and attributes silently; the paint check is what shows something was drawn. `Qt6::Svg` is found with `OPTIONAL_COMPONENTS`, and the test is
  built only `if(TARGET Qt6::Svg)`, so the tests of the other app that uses `libs` still
  configure without it.
- **Visual check:** a small desktop-only tool, `diagram_render <input> <output.svg> [width]
  [light|night] [pixelRatio]`, writes an `.svg` from a text file, prints the warnings and the
  scale, and is built next to `diagram_tests`. The spike uses it with a ratio of 3.

## Requirements for the later app integration

These are outside `libs/cpp/diagram`. The integration will be designed separately, but it has to
meet the following:

- the paste format carries the diagram text, either as a new block or inline in the description;
- the description references the diagram, and a preprocessor beside `RichTextImages` swaps in
  an `<img>` with the SVG for the current width and theme;
- the SVG travels as `data:image/svg+xml;utf8,` followed by the percent-encoded document, inside
  an `<img>` in a `RichText` text. This is the path `ColoredSvgProvider` (`libs/cpp/qmlutils`)
  already uses for the "revealed" icon in `ThemedHtmlText`, so it is proven on desktop and on
  Android, and the APK already carries the SVG image plugin. `ColoredSvgProvider` itself does not
  fit: it reads a file, paints everything in one colour by replacing `currentColor`, and
  overwrites `width`/`height` with the logical size. Its sibling `DiagramSvgProvider` takes the
  diagram `text`, `availableWidth`, `colors` (a map from palette role names such as
  `background` or `noteFill` to colours; missing roles keep the light defaults, other keys are
  ignored, and translucent colours keep their alpha through `fill-opacity`/`stroke-opacity`),
  `fontFamily`, `fontPixelSize` and `pixelRatio` (3, at least 1). It returns `svgSource`
  (`svgSourceChanged`), the logical `imageWidth` and `imageHeight` for the `<img>`
  (`imageSizeChanged`; scaled down when the scene overflows), the parse `error` and the
  `warnings`. It parses only when the text changes, lays out only when the width crosses a
  16 px step or the font changes, and rewrites the SVG only after a new layout or a change of
  colours or ratio; a width change within a step only resizes the image. The natural size is
  capped at 8 million pixels by lowering the ratio (never below 1), so a tall diagram cannot
  exhaust memory when rich text rasterizes it. A text that fails to parse clears the image and
  sets `error`; the caller shows the error instead of a stale picture;
- `RichTextImages::imageSize` cannot read `data:` URLs, so the preprocessor writes the `<img>`'s
  `width` and `height` itself (as `ThemedHtmlText` does), which `fit()` already respects;
- the width is rounded down to a multiple of 16 px before layout. Results are cached by (text, rounded
  width, theme, font), so a resize or a rotation does not re-lay out and re-compose on every pixel;
- `DiagramPalette` and `DiagramStyle` are built from `Theme`. `DiagramStyle` is the single source
  of the font for both the measurer and the writer;
- the import prompt gains a few lines on the supported syntax.

## Embedding in rich text

A diagram lives inline in an HTML text, between a line starting with `@startuml` and a line
starting with `@enduml`, each on a line of its own. `EmbeddedDiagrams::find` returns every block
with its character span, its line within the text and its body; a block with no `@enduml` runs to
the end and is marked unclosed. `EmbeddedDiagrams::locate` turns a parser error (`Line N: …`) into
a line of the surrounding text, so an importer can point at the line of the mistake. A body loses
its `\r` and the HTML escapes `&lt;`, `&gt;`, `&quot;` and `&amp;`, so an arrow written as `-&gt;`
by a tool that escapes everything still parses. `EmbeddedDiagrams::strayMarkers` lists the lines
that mention a marker without being one (`<p>@startuml</p>`), which `find` would silently skip.

`RichTextDiagrams` (QObject) takes `html`, `availableWidth`, `pixelRatio`, `fontFamily`,
`fontPixelSize` and `colors`, keeps one `DiagramSvgProvider` per block, and returns
`renderedHtml`: each block becomes `<p class="diagram" align="center"><img src="data:…" width
height></p>`, an error or an unclosed block becomes `<p class="diagramError">Diagram: …</p>`, and a
block shows nothing until the width is known. The style sheet should give `p.diagram` a
`line-height` of 100%: a proportional line height of the surrounding paragraphs would otherwise
multiply the image's height and leave a gap under it. The `Diagram:` text is an untranslated
fallback; an importer is expected to reject a broken block before it is ever shown. It runs after `RichTextImages`, so data URLs never reach its
size cache.

## Spike before integration

Whether a `data:` SVG shows in rich text, and whether the APK carries the SVG plugin, is already
settled by `ColoredSvgProvider`. One question is left: on an Android device with a DPR above 2,
is a diagram emitted with natural size W·3 and shown at W × H sharp? Also compare a label's
`QFontMetricsF` width against its rendered width, to confirm the 6% `textSafety` margin. If the
result is blurry despite the larger natural size, the fallback is a separate QML `Image`
between fragments of the description, which changes `FactDocument`. The spike is done with the
first integration build rather than a separate test screen.

## Plan

1. Skeleton: the directory, the CMake target and the test target, wired into `libs`.
2. Model and parser, including warnings and groups, with tests.
3. `Scene`, `TextMeasurer` with its fake, and `SequenceLayout`, with invariant tests.
4. `SvgSceneWriter`, with golden tests, the XML check, the `QSvgRenderer` smoke test and the
   visual check tool.
5. App integration (separate design), on the `ColoredSvgProvider` pattern.
6. The sharpness spike on a device, with the first integration build.
