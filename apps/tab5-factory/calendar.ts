// Calendar arithmetic for the RTC setting window (LVGL's lv_calendar shows
// six weeks starting on Sunday). Pure functions.
import { idiv, imod, push, type i32 } from "@pocketjs/framework/solid/std";

export interface DayCell {
  id: i32;
  day: i32;
  /** The cell belongs to the shown month (others are greyed). */
  current: boolean;
}

export function leapYear(year: i32): boolean {
  return (imod(year, 4) === 0 && imod(year, 100) !== 0) || imod(year, 400) === 0;
}

/** Days in `month` (1..12) of `year`. */
export function daysInMonth(year: i32, month: i32): i32 {
  if (month === 2) {
    return leapYear(year) ? 29 : 28;
  }
  if (month === 4 || month === 6 || month === 9 || month === 11) {
    return 30;
  }
  return 31;
}

/** Day of the week, 0 Sunday .. 6 Saturday (Sakamoto's method). */
export function weekday(year: i32, month: i32, day: i32): i32 {
  const offsets: i32[] = [0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4];
  const y: i32 = month < 3 ? year - 1 : year;
  return imod(y + idiv(y, 4) - idiv(y, 100) + idiv(y, 400) + offsets[month - 1] + day, 7);
}

/** The 42 cells of the month grid. */
export function monthCells(year: i32, month: i32): DayCell[] {
  const cells: DayCell[] = [];
  const first: i32 = weekday(year, month, 1);
  const days: i32 = daysInMonth(year, month);
  const previousMonth: i32 = month === 1 ? 12 : month - 1;
  const previousYear: i32 = month === 1 ? year - 1 : year;
  const previousDays: i32 = daysInMonth(previousYear, previousMonth);
  for (let i: i32 = 0; i < 42; i++) {
    const day: i32 = i - first + 1;
    if (day < 1) {
      push(cells, { id: i, day: previousDays + day, current: false });
    } else if (day > days) {
      push(cells, { id: i, day: day - days, current: false });
    } else {
      push(cells, { id: i, day: day, current: true });
    }
  }
  return cells;
}

/** Two digits with a leading zero. */
export function pad2(value: i32): string {
  return value < 10 ? `0${value}` : `${value}`;
}
