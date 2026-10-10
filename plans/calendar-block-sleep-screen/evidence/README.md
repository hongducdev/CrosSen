# Calendar Block sleep screen — capture evidence

`calendar-sleep.png` is the simulator's own framebuffer dump of the Calendar Block
sleep screen, captured headlessly on 2026-10-10 (host date) with:

```bash
WORK=/tmp/cal_capture
mkdir -p "$WORK/fs_/.crosspoint"
printf '%s' '{"sleepScreen": 12, "sleepTimeoutMinutes": 1}' \
  > "$WORK/fs_/.crosspoint/crossink-settings.json"
cd "$WORK"
CROSSPOINT_SIM_SCREENSHOTS="3000:$WORK/boot.bmp;75000:$WORK/calendar-sleep.bmp" \
  SDL_VIDEODRIVER=offscreen \
  timeout 110 "$REPO/.pio/build/simulator/program"
```

`sleepScreen: 12` is `CrossPointSettings::CALENDAR_SLEEP`; the one-minute timeout
makes the simulator enter sleep on its own, and `CROSSPOINT_SIM_SCREENSHOTS`
writes the panel contents at the 75 s mark. The UI language of the capture is the
simulator default (English), which is why the banner reads `October · 2026`;
every almanac string is Vietnamese by design.

## What the capture is checked against

The page date is the host date, 10/10/2026, whose lunar values are fixed by
`test/lunar_calendar/LunarCalendarTest.cpp` (09/10/2026 → 29/8) and by the engine
probe used for this review:

| Rendered | Expected |
| --- | --- |
| `Saturday` | Monday-first weekday index 5 |
| `10` | hero day digits (MSB-first 1bpp, set bit = white) |
| `Lunar: Mùng một · tháng Chín` | `solarToLunar(10, 10, 2026)` → 1/9/2026 |
| `Year Bính Ngọ` | lunar year 2026 |
| `Month Mậu Tuất · Day Đinh Tỵ` | month pillar of 1/9, day pillar of 10/10/2026 |
| `Auspicious hours: Sửu, Thìn, Ngọ, Mùi, Tuất, Hợi` | Tỵ day branch (index 5), row 5 |
| `Ngày Giải phóng Thủ đô` | 10/10 solar holiday |
| `— Tục ngữ Việt Nam` | quote author, right-aligned on the last line |

Vietnamese diacritics render correctly at every size, and no band overlaps the
card border or runs past its bottom edge.
