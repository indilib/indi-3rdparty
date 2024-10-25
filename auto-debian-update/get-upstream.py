#!/usr/bin/python3

import requests
import json
import os

# GitHub API variables
GITHUB_API = "https://api.github.com"
REPO_OWNER = "amprebonne"
REPO_NAME = "indi-3rdparty"
GITHUB_TOKEN = "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"

# Output file name
OUTPUT_FILE = "drivers_info.json"

# Set API endpoint and headers
endpoint = f"{GITHUB_API}/repos/{REPO_OWNER}/{REPO_NAME}/contents/"
headers = {
    "Authorization": f"Bearer {GITHUB_TOKEN}",
    "Accept": "application/vnd.github.v3+json"
}

def get_drivers_info():
    response = requests.get(endpoint, headers=headers)
    response.raise_for_status()

    drivers_info = []
    for item in response.json():
        if item["type"] == "dir" and item["name"].startswith("indi-"):
            drivers_info.append({"name": item["name"], "sha": item["sha"]})

    with open(OUTPUT_FILE, "w") as f:
        json.dump(drivers_info, f, indent=4)

def check_repo_exists():
    endpoint = f"{GITHUB_API}/repos/{REPO_OWNER}/{REPO_NAME}"
    response = requests.get(endpoint, headers=headers)
    return response.status_code == 200

def handle_error(code, message):
    print(message)
    exit(code)

def main():
    if not check_repo_exists():
        handle_error(1, "GitHub repository error: repository not found or inaccessible")

    get_drivers_info()

    # Check if output file was created and is not empty
    if not os.path.getsize(OUTPUT_FILE) > 0:
        handle_error(1, "No matching directories found or output file is empty")

if __name__ == "__main__":
    main()
