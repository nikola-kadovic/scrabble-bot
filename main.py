import sys
import os

# Add quackle bindings to Python path
bindings_path = os.path.join(os.path.dirname(__file__), 'quackle', 'bindings', 'python3')
if bindings_path not in sys.path:
    sys.path.insert(0, bindings_path)

from gaddag import Gaddag
import quackle

def main():
    gaddad = Gaddag(wordlist_path=os.path.join(os.path.dirname(__file__), 'dictionary', 'nwl_2023.txt'))

    gaddad.load_wordlist()

if __name__ == "__main__":
    main()
