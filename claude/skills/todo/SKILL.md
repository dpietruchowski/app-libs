---
name: todo
description: Manage a prioritized project TODO list stored in .claude/TODO.md. Use when the user wants to add tasks, mark items done, view the list, or reorganize priorities. Pass a natural-language instruction as argument (e.g. "add: implement login screen [high]", "done: login screen", "show", "clear done").
allowed-tools: Read, Edit, Write
---

## Manage project TODO list

The list is stored in `.claude/TODO.md`. It is divided into three priority sections: **High**, **Medium**, and **Low**. Each item is a markdown checkbox (`- [ ]` or `- [x]`).

### File format

```markdown
# Project TODO

## High Priority
- [ ] Task description

## Medium Priority
- [ ] Task description

## Low Priority
- [ ] Task description
```

### Determining the action

Parse `$ARGUMENTS` (or the latest user message if `$ARGUMENTS` is empty) to determine intent:

| User intent | Action |
|---|---|
| Empty / "show" / "lista" / "co mamy" | Display the current list |
| "add: …" / "dodaj: …" / "nowe: …" | Add a new item |
| "done: …" / "zrobione: …" / "gotowe: …" / "skończyłem …" / "już jest …" | Mark matching item(s) as `[x]` |
| "undone: …" / "cofnij: …" | Mark matching item(s) back to `[ ]` |
| "remove: …" / "usuń: …" | Remove specific item |
| "clear done" / "wyczyść zrobione" / "usuń zrobione" | Remove all `[x]` items |
| "prioritize: … [high/medium/low]" / "przenieś: … [high/medium/low]" | Move item to a different priority section |

If intent is ambiguous, ask the user before modifying the file.

### Steps

1. **Read the file.** Use the Read tool on `.claude/TODO.md`. If the file does not exist yet, create it with the skeleton format above (all three sections, empty).

2. **Execute the action:**

   **Show:** Print the file contents formatted as markdown. Count open items per priority and show a summary line: `X tasks open (H high, M medium, L low), Y done`.

   **Add:** Determine priority from keywords in the input:
   - `[high]` / `[h]` / `wysoki` / `pilne` / `ważne` → High Priority
   - `[low]` / `[l]` / `niski` / `kiedyś` / `nice to have` → Low Priority
   - default → Medium Priority

   Append the item as `- [ ] <description>` under the correct section. Strip priority tags from the stored description.

   **Mark done:** Find the item whose description best matches the input (case-insensitive substring match). Change `- [ ]` to `- [x]`. If multiple items match, list them and ask the user to confirm which one.

   **Mark undone:** Same matching logic, but change `- [x]` back to `- [ ]`.

   **Remove:** Find matching item(s), show them, ask for confirmation, then delete the line(s).

   **Clear done:** Remove all lines starting with `- [x]`. Ask for confirmation first.

   **Prioritize/move:** Find the item, remove it from its current section, append it to the target section.

3. **Write the result** with the Edit or Write tool (prefer Edit for small changes).

4. **Confirm** what was changed in one line, e.g.:  
   `Marked done: "Implement login screen" (was in High Priority)`  
   `Added to Medium Priority: "Refactor ViewModel"`

### Don't

- Do not silently create a new item when trying to mark something done — if nothing matches, say so and ask.
- Do not reorder or reformat lines that were not touched.
- Do not remove the section headers, even if a section is empty.
- Do not commit the file — leave that to the user or the `commit` skill.
- Do not add timestamps, owners, or IDs unless the user asks.
