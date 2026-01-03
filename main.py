import sys
import os

# Add quackle bindings to Python path
bindings_path = os.path.join(os.path.dirname(__file__), 'quackle', 'bindings', 'python3')
if bindings_path not in sys.path:
    sys.path.insert(0, bindings_path)

import quackle

def main():
    print("Hello from scrabble-bot!")

    dm = quackle.DataManager()

    dm.setAppDataDirectory("data")


if __name__ == "__main__":
    main()
