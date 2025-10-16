import sqlite3


location_suffixes = {
  ("T"): "Tôn Thất Thuyết",
  ("A", "B"): "Kiều Mai"
}
rooms = "101-T|102-T|103-A|104-T|105-T|106-A|106-T|107-T|108-B|109-B|110-B|201-A|201-B|202-A|202-B|203-A|203-B|203-T|204-A|204-B|204-T|205-A|205-B|205-T|206-A|206-B|206-T|207-A|207-B|207-T|208-B|208-T|209-B|209-T|210-B|210-T|211-T|213-T|214-T|215-T|216-T|217-T|301-A|302-A|303-A|304-A|304-B|305-A|306-A|307-A|308-B|402-A|402-B|403-A|404-A|405-A|406-A|406-B|407-A|407-B|408-B|409-B|410-B|501-B|502-B|503-B|504-B|505-B|506-B|507-B|508-B|509-B|510-B"


if __name__ == "__main__":
  rows = []
  for room_code in rooms.split('|'):
    for k, v in location_suffixes.items():
      if any(room_code.endswith(c) for c in k):
        rows.append((room_code, v))
        break
  print("Adding rooms to database.db...")
  with sqlite3.connect("database.db") as db:
    cur = db.cursor()
    cur.executescript("""
      DROP TABLE IF EXISTS rooms;
      CREATE TABLE rooms (
        room_code TEXT PRIMARY KEY,
        location  TEXT
      );
    """)
    cur.executemany("INSERT INTO rooms VALUES (?, ?)", rows)
  print("Done!")