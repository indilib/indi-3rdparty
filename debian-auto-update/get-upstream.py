#!/usr/bin/python3

import requests
import json
import os
import logging
import base64
from typing import Dict, List

# GitHub API variables
GITHUB_API = "https://api.github.com"
REPO_OWNER = "amprebonne"
REPO_NAME = "indi-3rdparty"
GITHUB_TOKEN = "---access-token---"
DEBIAN_DIR = "debian"

# Output file name
OUTPUT_FILE = "upstream-data.json"

# Error codes
ERROR_REPO_NOT_FOUND = 1
ERROR_UNKNOWN = 7

# Set API endpoint and headers
endpoint = f"{GITHUB_API}/repos/{REPO_OWNER}/{REPO_NAME}/contents/{DEBIAN_DIR}"
headers = {
    "Authorization": f"Bearer {GITHUB_TOKEN}",
    "Accept": "application/vnd.github.v3+json"
}

# Set logging level to INFO
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

def get_drivers_info() -> List[Dict]:
    """
    Retrieves upstream drivers data from debian directory.
    """
    drivers_info = []
    response = requests.get(endpoint, headers=headers)
    response.raise_for_status()

    for item in response.json():
        if item["type"] == "dir" and (item["name"].startswith("indi") or item["name"].startswith("lib")):
            driver_name = item["name"]

            # Get changelog content
            changelog_endpoint = f"{GITHUB_API}/repos/{REPO_OWNER}/{REPO_NAME}/contents/{DEBIAN_DIR}/{driver_name}/changelog"
            changelog_response = requests.get(changelog_endpoint, headers=headers)

            if changelog_response.status_code == 404:
                logging.warning(f"Changelog not found for {driver_name}")
                continue

            changelog_response.raise_for_status()
            changelog_data = changelog_response.json()
            logging.info(f"Retrieved changelog data for {driver_name}")  # Updated log level

            # Extract data from first line of changelog
            changelog_content = base64.b64decode(changelog_data["content"]).decode("utf-8")
            first_line = changelog_content.splitlines()[0]
            logging.info(f"Extracted first line of changelog for {driver_name}: {first_line}")  # Updated log level

            # Extract driver name and version
            parts = first_line.split(" ")
            driver_name_from_changelog = parts[0]
            if len(parts) > 1:
                version = parts[1].strip("()")
            else:
                version = "Unknown"

            # Get last commit hash for changelog file
            commits_endpoint = f"{GITHUB_API}/repos/{REPO_OWNER}/{REPO_NAME}/commits?path={DEBIAN_DIR}/{driver_name}/changelog&per_page=1"
            commits_response = requests.get(commits_endpoint, headers=headers)
            commits_response.raise_for_status()
            commits_data = commits_response.json()
            logging.info(f"Retrieved commit data for {driver_name} changelog")  # Updated log level

            if isinstance(commits_data, list) and commits_data:
                last_commit_hash = commits_data[0]["sha"]
            else:
                last_commit_hash = "No commits found"

            drivers_info.append({
                "name": driver_name_from_changelog,
                "version": version,
                "hash": last_commit_hash
            })

    with open(OUTPUT_FILE, "w") as f:
        json.dump(drivers_info, f, indent=4)


def check_repo_exists() -> bool:
    """
    Checks if GitHub repository exists.
    """
    endpoint = f"{GITHUB_API}/repos/{REPO_OWNER}/{REPO_NAME}"
    response = requests.get(endpoint, headers=headers)
    return response.status_code == 200


def handle_error(code: int, message: str) -> None:
    """
    Handles error and exits.
    """
    logging.error(message)
    exit(code)


def main() -> None:
    """
    Main function.
    """
    if not check_repo_exists():
        handle_error(ERROR_REPO_NOT_FOUND, "GitHub repository error: repository not found or inaccessible")

    try:
        get_drivers_info()
    except requests.RequestException as e:
        handle_error(ERROR_UNKNOWN, f"Request error: {e}")
    except json.JSONDecodeError as e:
        handle_error(ERROR_UNKNOWN, f"JSON decode error: {e}")
    except Exception as e:
        handle_error(ERROR_UNKNOWN, f"Unknown error: {e}")

    # Check if output file was created and is not empty
    if not os.path.getsize(OUTPUT_FILE) > 0:
        handle_error(ERROR_REPO_NOT_FOUND, "No matching directories found or output file is empty")


if __name__ == "__main__":
    main()
