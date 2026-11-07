# Part Public License (PPL)
# Copyright (C) 2026 Abdullah Al Nahian/Raiyan
# View LICENSE.md

# Part Public License (PPL)
# Copyright (C) <Year> <Author_Name>

import os
import shutil
import subprocess

def copy_directories_only(source_dir, destination_dir, script_dir):
    """
    Recursively copies the folder structure from source_dir to destination_dir
    without copying files, the build folder, or the script's home directory.
    """
    abs_dest = os.path.abspath(destination_dir)
    abs_script = os.path.abspath(script_dir)
    abs_source = os.path.abspath(source_dir)

    def is_git_ignored(path):
        relative_path = os.path.relpath(path, abs_source)
        result = subprocess.run(
            ["git", "check-ignore", "--quiet", "--", relative_path],
            cwd=abs_source,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        return result.returncode == 0

    def has_copyable_files(directory):
        """Return whether a directory contains any file not excluded by policy."""
        for current_dir, subdirs, files in os.walk(directory):
            # Do not descend into directories that are excluded independently.
            subdirs[:] = [
                name for name in subdirs
                if os.path.abspath(os.path.join(current_dir, name)) != abs_dest
                and os.path.abspath(os.path.join(current_dir, name)) != abs_script
                and os.path.relpath(os.path.join(current_dir, name), abs_source)
                not in ("include", "tools", ".devcontainer", "lib")
                and not is_git_ignored(os.path.join(current_dir, name))
            ]
            for name in files:
                if not is_git_ignored(os.path.join(current_dir, name)):
                    return True
        return False

    def ignore_items(current_dir, directory_contents):
        ignored = []
        for item in directory_contents:
            full_path = os.path.join(current_dir, item)
            abs_item = os.path.abspath(full_path)
            
            if os.path.isfile(full_path):
                ignored.append(item)
            elif abs_item == abs_dest:
                ignored.append(item)
            elif abs_item == abs_script:
                ignored.append(item)
            elif os.path.relpath(abs_item, abs_source) in ("include", "tools", "lib", ".devcontainer"):
                ignored.append(item)
            elif is_git_ignored(abs_item):
                ignored.append(item)
            elif not has_copyable_files(abs_item):
                ignored.append(item)

        return ignored

    shutil.copytree(source_dir, destination_dir, ignore=ignore_items, dirs_exist_ok=True)

if __name__ == "__main__":
    # Dynamically find the folder where this script file lives
    # (Falls back to the current directory if run in an interactive shell)
    SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__)) if '__file__' in locals() else os.getcwd()
    
    SOURCE_PATH = "."           # Root source folder
    BUILD_PATH = "./build"      # Destination build folder
    
    try:
        copy_directories_only(SOURCE_PATH, BUILD_PATH, SCRIPT_DIR)
    except Exception as e:
        os.abort()