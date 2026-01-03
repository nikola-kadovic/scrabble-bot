download_wordlist: get_cs21 get_nwl_2023

get_cs21:
	wget -O dictionary/csw21.txt https://raw.githubusercontent.com/scrabblewords/scrabblewords/refs/heads/main/words/British/CSW21.txt

get_nwl_2023:
	wget -O dictionary/nwl_2023.txt https://raw.githubusercontent.com/scrabblewords/scrabblewords/refs/heads/main/words/North-American/NWL2023.txt