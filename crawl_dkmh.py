import collections, re, requests, sqlite3


def get_term_code(year: str, term: str) -> str:
  """
  Get term code from academic year and term.  
  Args:
    year (str): last 2 digits of the academic year (e.g. `25` for 2025).  
    term (str): can be `1`, `2` or `3` (the summer term).  
  """
  code = 3 * int(year) + int(term) - 32
  return str(code).rjust(3, '0')


def get_course_code(course_section_code: str) -> str:
  """
  Get Course code from Course section code.  
  e.g. Get `INT2211` from `INT2211 11`.  
  """
  # Some course codes are compact (e.g., "INT2211"), others are spaced (e.g., "INT 3103")
  # Combine parts from the left until the first one containing a number is found
  course_code = ""
  for part in course_section_code.split():
    course_code += part
    if any(x.isnumeric() for x in part):
      return course_code


def get_course_registrations(year: str, term: str) -> None:
  """
  Get all course registration records and save to `database.db`.
  """
  DANGKYHOC_URL = "http://112.137.129.87/qldt?SinhvienLmh[term_id]={}&SinhvienLmh_page={}&pageSize=25000"
  DANGKYHOC_COUNT_PATTERN = r'Kết quả từ \d+ tới (\d+) trên (\d+).'
  DANGKYHOC_RECORD_PATTERN = r'<td style="width: 20px">(.+)</td><td style="width: 40px">(.+)</td><td style="width: 100px">.+</td><td style="width: 60px">.+</td><td style="width: 100px">.+</td><td style="width: 50px">(.+)</td><td style="width: 160px">(.+)</td><td style="width: 15px">(.+)</td><td style="width: 15px">(.+)</td><td style="width: 60px">.+</td><td style="width: 140px; display:none">.+</td>'
  course_info = collections.defaultdict(lambda: {"name": "", "credits": 0, "has_practice": 0, "sections": set()})
  term_code = get_term_code(year, term)
  dkmh_rows = []
  for page in range(1, 10000):
    print(f"Fetching page {page}...")
    data = requests.get(DANGKYHOC_URL.format(term_code, page)).text
    for i, student_id, course_section_code, course_name, group, credits in re.findall(DANGKYHOC_RECORD_PATTERN, data):
      print(f"Processing record #{i}...")
      # print(i, student_id, course_section_code, credits, group)
      if not student_id or not course_section_code \
          or any(t in course_section_code for t in ["mien", "PES"]):
        continue
      course_code = get_course_code(course_section_code)
      if course_info[course_code]["credits"] == 0:
        course_info[course_code]["name"] = course_name
        course_info[course_code]["credits"] = credits
      course_info[course_code]["has_practice"] |= (group != "CL")
      course_info[course_code]["sections"].add(course_section_code)
      dkmh_rows.append((student_id, course_code))
    last, count = t.groups() if (t := re.search(DANGKYHOC_COUNT_PATTERN, data)) else (0, 0)
    if last == count:
      break
  print("Saving to database.db...")
  with sqlite3.connect("database.db") as db:
    cur = db.cursor()
    cur.executescript("""
      PRAGMA journal_mode=WAL;
      PRAGMA synchronous=NORMAL;
      PRAGMA temp_store=MEMORY;
      DROP TABLE IF EXISTS courses;
      CREATE TABLE courses (
        course_code    TEXT    PRIMARY KEY,
        course_name    TEXT,
        credits        INTEGER,
        has_practice   BOOLEAN,
        sections_count INTEGER
      );
      DROP TABLE IF EXISTS students;
      CREATE TABLE students (
        student_id  TEXT,
        course_code TEXT,
        UNIQUE (student_id, course_code),
        FOREIGN KEY (course_code) REFERENCES courses(course_code)
      );
    """)
    lmh_rows = [(c, course_info[c]["name"], course_info[c]["credits"], course_info[c]["has_practice"], len(course_info[c]["sections"])) for c in course_info]
    cur.executemany("INSERT INTO courses VALUES (?, ?, ?, ?, ?)", lmh_rows)
    cur.executemany("INSERT INTO students VALUES (?, ?)", dkmh_rows)
  print("Done!")


if __name__ == "__main__":
  year = input('Enter year (e.g. "25" for academic year 2025-2026): ')
  term = input('Enter term ("1", "2" or "3" (the summer term)): ')
  get_course_registrations(year, term)
