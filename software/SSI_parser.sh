#!/usr/bin/bash
#Parses the files in the given directory and outputs with SSI performed
#Inspired by https://stackoverflow.com/questions/18377549/i-need-a-script-that-searches-files-for-ssi-and-replaces-the-include-with-the-ac

search=${1:-./}

replace() {
  while read -r x; do
    if [[ "$x" =~ \<!--#include\ file=\"([^\.]+.html)\"--\> ]]; then
      cat "${BASH_REMATCH[1]}";
    else
      echo "$x"
    fi
  done <"$1"
}


while read f; do
  echo "parsing $f for SSI"
  replace "$f" > tmp_$$.tmp && mv tmp_$$.tmp "$f"
done < <(find $search  -maxdepth 1 -name '*.html')
