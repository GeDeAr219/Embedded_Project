import sys
import csv
from datetime import datetime
from database import FaceDatabase


def print_logs(logs):
    if not logs:
        print("No logs found.")
        return

    print(f"\n{'ID':>4} | {'Time':<20} | {'User':<15} | {'Status':<10} | {'Distance'}")
    print("-" * 68)

    for row in logs:
        log_id, user_name, status, distance, created_at = row
        user_str    = (user_name or "-")[:15]
        dist_str    = f"{distance:.4f}" if distance is not None else "-"
        status_icon = "OK" if status == "MATCH" else "!!"
        print(f"{log_id:>4} | {created_at:<20} | {user_str:<15} | "
              f"{status_icon} {status:<8} | {dist_str}")

    print(f"\nTotal: {len(logs)} entries\n")


def export_csv(db, filename="access_log_export.csv"):
    logs = db.get_logs(limit=10000)
    with open(filename, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["ID", "User", "Status", "Distance", "Timestamp"])
        writer.writerows(logs)
    print(f"[EXPORT] Saved {len(logs)} rows to '{filename}'")


def filter_by_status(db, status):
    all_logs = db.get_logs(limit=10000)
    return [row for row in all_logs if row[2] == status]


def interactive_menu(db):
    while True:
        print("\n==== ACCESS LOG VIEWER ====")
        print("  1. Show last 20 entries")
        print("  2. Show last 100 entries")
        print("  3. Show MATCH entries only")
        print("  4. Show DENIED / NO_MATCH entries only")
        print("  5. Export all to CSV")
        print("  6. Exit")
        print("===========================")
        choice = input("Select: ").strip()

        if choice == "1":
            print_logs(db.get_logs(20))
        elif choice == "2":
            print_logs(db.get_logs(100))
        elif choice == "3":
            print_logs(filter_by_status(db, "MATCH"))
        elif choice == "4":
            no_match = filter_by_status(db, "NO_MATCH")
            no_face  = filter_by_status(db, "NO_FACE")
            print_logs(no_match + no_face)
        elif choice == "5":
            ts = datetime.now().strftime("%Y%m%d_%H%M%S")
            export_csv(db, f"log_{ts}.csv")
        elif choice == "6":
            break
        else:
            print("Invalid choice.")


if __name__ == "__main__":
    db = FaceDatabase()

    # show last 50
    if len(sys.argv) == 2 and sys.argv[1].isdigit():
        print_logs(db.get_logs(int(sys.argv[1])))

    elif len(sys.argv) == 2 and sys.argv[1] == "export":
        export_csv(db)

    else:
        interactive_menu(db)