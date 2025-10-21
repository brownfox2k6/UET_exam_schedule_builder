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
  DANGKYHOC_RECORD_PATTERN = r'<td style="width: 20px">(.+)</td><td style="width: 40px">(.+)</td><td style="width: 100px">.+</td><td style="width: 60px">.+</td><td style="width: 100px">.+</td><td style="width: 50px">(.+)</td><td style="width: 160px">(.+)</td><td style="width: 15px">.+</td><td style="width: 15px">.+</td><td style="width: 60px">.+</td><td style="width: 140px; display:none">.+</td>'
  term_code = get_term_code(year, term)
  courses = dict()
  sections = dict()
  enrolments = []
  for page in range(1, 10000):
    print(f"Fetching page {page}...")
    data = requests.get(DANGKYHOC_URL.format(term_code, page)).text
    for i, student_id, section_code, course_name in re.findall(DANGKYHOC_RECORD_PATTERN, data):
      print(f"Processing record #{i}...")
      if not student_id or not section_code \
          or any(t in section_code for t in ["mien", "PES"]):
        continue
      course_code = get_course_code(section_code)
      courses[course_code] = course_name
      sections[section_code] = course_code
      enrolments.append((student_id, section_code))
    last, count = t.groups() if (t := re.search(DANGKYHOC_COUNT_PATTERN, data)) else (0, 0)
    if last == count:
      break
  print("Saving to database.db...")
  with sqlite3.connect("database.db") as db:
    cur = db.cursor()
    cur.executescript("""
      DROP TABLE IF EXISTS courses;
      CREATE TABLE courses (
        course_code  TEXT  PRIMARY KEY,
        course_name  TEXT
      );
      DROP TABLE IF EXISTS sections;
      CREATE TABLE sections (
        section_code  TEXT  PRIMARY KEY,
        course_code   TEXT,
        FOREIGN KEY (course_code) REFERENCES courses (course_code)
      );
      DROP TABLE IF EXISTS enrolments;
      CREATE TABLE enrolments (
        student_id    TEXT,
        section_code  TEXT,
        FOREIGN KEY (section_code) REFERENCES sections (section_code),
        UNIQUE (student_id, section_code)
      );
    """)
    cur.executemany("INSERT INTO courses VALUES (?, ?)", courses.items())
    cur.executemany("INSERT INTO sections VALUES (?, ?)", sections.items())
    cur.executemany("INSERT INTO enrolments VALUES (?, ?)", enrolments)
  print("Done!")


if __name__ == "__main__":
  year = input('Enter year (e.g. "25" for academic year 2025-2026): ')
  term = input('Enter term ("1", "2" or "3" (the summer term)):     ')
  get_course_registrations(year, term)
