#!/usr/bin/env python3

from pathlib import Path
import subprocess

# Get the current directory (path of the script)
current_directory = Path(__file__).resolve().parent

# Move up one level from 'scripts' to reach the root of the project
root_directory = current_directory.parent

# Define the path where drivers are located
drivers_directory_path = root_directory

print("drivers_directory_path: ", drivers_directory_path)
# Get a list of all directories in the drivers folder
def list_drivers(drivers_directory_path):

    """
    List all drivers in the drivers directory.
    Args:
        drivers_directory_path (Path): Path to the drivers directory.
    Returns: 
        list: List of driver names.
    """
    try:
        drivers = [item.name for item in drivers_directory_path.iterdir() if item.is_dir() and item.name.startswith("indi-")]
        return drivers
    except Exception as e:
        print(f"Error occurred while listing drivers: {e}")
        return []    

# Function to extract version from the changelog file
def extract_version_from_changelog(changelog_path):
    """
    Extract the driver version from the changelog file.
    Args:
        changelog_path (Path): Path to the changelog file.
    Returns:
        str: Extracted version, or 'Version not found'.
    """
    if changelog_path.exists():
        try:
            with changelog_path.open() as changelog_file:
                # The version is typically found in the first line in parentheses
                first_line = changelog_file.readline().strip()
                if first_line:
                    # Extract version from first line
                    version = first_line.split()[1].strip("()")
                    return version
        except Exception as e:
            print(f"Error reading version from {changelog_path}: {e}")
    return "not found"

# Function to get the latest git commit hash for a specific driver directory
def get_latest_git_hash(driver_path):
    """
    Get the latest git hash for the driver directory.
    Args:
        driver_path (Path): Path to the driver directory.
    Returns:
        str: Latest git hash, or 'Hash not found'.
    """
    try:
        # Run git log command to get the latest commit hash for the driver
        result = subprocess.run(
            ['git', 'log', '-n', '1', '--pretty=format:%H', '--', str(driver_path)],
            cwd=root_directory,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        if result.returncode == 0:
            return result.stdout.strip()
        else:
            print(f"Error occurred while getting git hash for {driver_path}: {result.stderr}")
            return "Hash not found"
    except Exception as e:
        print(f"Error running git command: {e}")
        return "Hash not found"

# Function to get version and git hash for each driver
def get_driver_info(drivers_directory_path):
    """
    Get the version and git hash for each driver in the debian directory and track the number of drivers.
    Args:
        drivers_directory_path (Path): Path to the debian directory.
    """
    drivers_list = list_drivers(drivers_directory_path)
    driver_count = 0  # Counter to keep track of the number of drivers
    
    if drivers_list:
        print("Drivers and their versions and git hashes:")
        for driver in drivers_list:
            # Define the path to the changelog for version extraction
            changelog_path = drivers_directory_path / driver / 'changelog'
            version = extract_version_from_changelog(changelog_path)
            
            # Get the latest git hash for the driver
            git_hash = get_latest_git_hash(drivers_directory_path / driver)
            
            # Print the driver, its version, and its latest git hash
            print(f"{driver}: Version {version}, Latest Git Hash: {git_hash}")
            
            # Increment the driver count
            driver_count += 1

        
        print(f"\nTotal number of drivers processed: {driver_count}")
    else:
        print("No drivers found or an error occurred.")

if __name__ == "__main__":
    get_driver_info(drivers_directory_path)