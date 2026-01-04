from dictionary.gaddag import Gaddag
import sys
import os

# Add quackle bindings to Python path
# bindings_path = os.path.join(os.path.dirname(__file__), 'quackle', 'bindings', 'python3')
# if bindings_path not in sys.path:
#     sys.path.insert(0, bindings_path)


def main():
    gaddag = Gaddag(
        wordlist_path=os.path.join(
            'dictionary',
            'nwl_2023.txt'))
    # build_gaddag() will automatically check for cache and use it if available
    gaddag.build_gaddag(use_cache=True)

    print("done")


if __name__ == "__main__":
    main()
