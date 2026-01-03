import sys
import os

# Add quackle bindings to Python path
bindings_path = os.path.join(os.path.dirname(__file__), 'quackle', 'bindings', 'python3')
if bindings_path not in sys.path:
    sys.path.insert(0, bindings_path)

import quackle

def main():
    print("Hello from scrabble-bot!")
    print(f"✓ Quackle module loaded successfully!")
    print(f"✓ Quackle version/attributes available: {len([x for x in dir(quackle) if not x.startswith('_')])} items")
    
    # Test basic functionality
    try:
        dm = quackle.DataManager()
        print(f"✓ DataManager created successfully")
    except Exception as e:
        print(f"✗ Error creating DataManager: {e}")

if __name__ == "__main__":
    main()
