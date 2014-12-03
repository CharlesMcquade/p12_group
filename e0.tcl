#!/usr/bin/expect

expect "*** System Shutdown ***\r"
send \001
send "x"
