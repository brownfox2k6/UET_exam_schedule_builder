import csv, sqlite3
from crawl_dkmh import get_course_code

if __name__ == "__main__":
  print("Extracting data from .csv file...")
  rows = []
  for line in csv.reader(open("timetable.csv", encoding="utf-8")):
    course_code = get_course_code(line[2])
    class_type = line[4].strip()
    if class_type not in ("LT", "TH"):
      continue
    for teacher in [s.strip() for s in line[5].split('+')]:
      rows.append((teacher, course_code, class_type))
  print("Adding teachers to database.db...")
  with sqlite3.connect("database.db") as db:
    cur = db.cursor()
    cur.executescript("""
      DROP TABLE IF EXISTS teachers;
      CREATE TABLE teachers (
        name        TEXT,
        course_code TEXT,
        class_type  TEXT,
        UNIQUE (name, course_code, class_type)
      );
    """)
    cur.executemany("INSERT OR IGNORE INTO teachers VALUES (?, ?, ?)", rows)
  print("Done!")