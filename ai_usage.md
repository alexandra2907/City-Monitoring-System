# AI Usage Report - Phase 1

**Tool Used:** Gemini AI
**Prompt:** "Am o structură în C pentru un raport care arată așa: int report_id, char inspector_name[32], float latitude, float longitude, char category[16], int severity, time_t timestamp, char description[136]. Te rog generează-mi două funcții: parse_condition și match_condition."

**AI Output:** Provided two functions using `sscanf` for parsing and a series of `if-else` blocks with `strcmp`, `atoi`, and `atol` for condition matching.

**Changes made:** I integrated the functions into `city_manager.c` and hooked them into the `filter` command logic to iterate through multiple command-line conditions.