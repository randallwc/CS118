X="805167986"

TAR="$X.tar.gz"

rm -f http-get

tar -xzvf $TAR; echo -e "$? == 0 untarring"

make

echo -e "$(head -n 1 README) == YOUR_NAME"

echo -e "$(head -n 2 README | tail -n +2) == $X"

echo -e "$(wc -l README) == 2 README"

# -a with bad argument and -c
./http-get -a bad -c 2> /dev/null; echo -e "$? == 1"

# -a with bad argument
./http-get -a bad 2> /dev/null; echo -e "$? == 1"

# no -a argument with -c
./http-get -c 2> /dev/null; echo -e "$? == 2"

# -a with no argument
./http-get -a 2> /dev/null; echo -e "$? == 2"

# no arguments
./http-get 2> /dev/null; echo -e "$? == 2"

# ok use of http-get
./http-get -a http://www.google.com/ 2> /dev/null > /dev/null; echo -e "$? == 0"

# example they gave us
./http-get -a  http://ucla.edu > test1.log 2> /dev/null; echo -e "$? == 0"
curl -v  http://ucla.edu > curl1.log 2>&1

./http-get -a  http://ucla.edu -c > test2.log 2> /dev/null; echo -e "$? == 0"
echo -en "GET / HTTP/1.1\r\nHost: ucla.edu\r\nUser-Agent: http-get 1.0\r\n\r\n" > ucla.log
