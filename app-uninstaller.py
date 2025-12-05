import subprocess
import sys

def get_installed_apps():
    """Gets a list of installed applications from dpkg."""
    try:
        # Get the list of installed packages
        process = subprocess.run(
            ["dpkg", "--get-selections"],
            capture_output=True,
            text=True,
            check=True
        )
        # Filter out packages that are not fully installed
        installed_apps = [
            line.split()[0]
            for line in process.stdout.strip().split("\n")
            if "install" in line.split()
        ]
        return installed_apps
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"Error getting installed applications: {e}", file=sys.stderr)
        sys.exit(1)

def main():
    """Main function to list and uninstall applications."""
    apps = get_installed_apps()

    # Display the list of applications
    print("Installed applications:")
    for i, app in enumerate(apps):
        print(f"{i + 1}. {app}")

    # Prompt the user to select an application
    while True:
        try:
            selection = input("Enter the number of the application to uninstall (or 'q' to quit): ")
            if selection.lower() == 'q':
                sys.exit(0)
            selection = int(selection)
            if 1 <= selection <= len(apps):
                selected_app = apps[selection - 1]
                break
            else:
                print("Invalid selection. Please try again.")
        except ValueError:
            print("Invalid input. Please enter a number.")

    # Confirm the uninstallation
    confirm = input(f"Are you sure you want to uninstall {selected_app}? (y/n): ")
    if confirm.lower() == 'y':
        # Generate and execute the uninstallation command
        try:
            subprocess.run(
                ["sudo", "apt-get", "remove", "-y", selected_app],
                check=True
            )
            print(f"{selected_app} has been uninstalled.")
        except (subprocess.CalledProcessError, FileNotFoundError) as e:
            print(f"Error uninstalling {selected_app}: {e}", file=sys.stderr)
            sys.exit(1)
    else:
        print("Uninstallation canceled.")

if __name__ == "__main__":
    main()
