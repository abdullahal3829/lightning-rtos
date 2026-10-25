import os
import shutil
import sys

def copy_directories_only(source_dir, destination_dir, script_dir):
    """
    Recursively copies the folder structure from source_dir to destination_dir
    without copying files, the build folder, or the script's home directory.
    """
    abs_dest = os.path.abspath(destination_dir)
    abs_script = os.path.abspath(script_dir)

    def ignore_items(current_dir, directory_contents):
        ignored = []
        for item in directory_contents:
            full_path = os.path.join(current_dir, item)
            abs_item = os.path.abspath(full_path)
            
            # 1. Ignore if it's a file
            if os.path.isfile(full_path):
                ignored.append(item)
            
            # 2. Ignore if it matches the destination build directory
            elif abs_item == abs_dest:
                ignored.append(item)
                
            # 3. Ignore if it matches the directory where the script lives
            elif abs_item == abs_script:
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