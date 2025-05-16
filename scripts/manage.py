from openai import OpenAI
import argparse
from dotenv import load_dotenv
import re
from datetime import datetime

load_dotenv()
client = OpenAI()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Manage OpenAI files.")
    parser.add_argument("action", choices=["show", "delete"], help="Action to perform on files.")
    parser.add_argument("--ids", nargs="*", help="ID(s) of the file(s) to perform action on.")
    parser.add_argument("--all", action="store_true", help="Perform action on all files.")
    parser.add_argument("--pattern", type=str, help="Regular expression to match file names.")
    parser.add_argument("--limit", type=int, default=-1, help="Limit the number of files to show.")
    args = parser.parse_args()
    
    if sum(bool(x) for x in [args.pattern, args.all, args.ids]) != 1:
        parser.error("Specify exactly one of --pattern, --all, or --ids.")
    
    files = client.files.list()
    
    if args.pattern:
        pattern = re.compile(args.pattern)
        files = [file for file in files if pattern.search(file.filename)]
    elif args.ids:
        files = [file for file in files if file.id in args.ids]

    if args.limit > 0:
        files = files[:args.limit]

    if args.action == "show":
        for file in files:
            created_time = datetime.fromtimestamp(file.created_at).strftime('%Y-%m-%d %H:%M:%S')
            print(f"{file.filename}:\n  ID: {file.id}\n  File Size: {file.bytes}\n  Created At: {created_time}")
    elif args.action == "delete":
        for file in files:
            if client.files.delete(file.id).deleted:
                print(f"Deleted file: {file.filename}")
            else:
                print(f"Failed to delete file: {file.filename}")
    