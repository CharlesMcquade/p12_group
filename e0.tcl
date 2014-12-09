
#!/usr/bin/expect

set timeout 10

expect_after {
    timeout {
        puts "----> timeout <----\r"
        exit
    }
}

spawn qemu-system-x86_64 -nographic --serial mon:stdio -hdc kernel/kernel.img -hdd fat439/user.img

expect "shell:/$ "
send "ls\r"

expect "shell:/$ "
send "mkdir DIR1\r"

expect "shell:/$ "
send "mkdir DIR2\r"

expect "shell:/$ "
send "mkdir DIR3\r"

expect "shell:/$ "
send "mkdir DIR4\r"

expect "shell:/$ "
send "mkdir DIR5\r"

expect "shell:/$ "
send "mkdir DIR6\r"

expect "shell:/$ "
send "mkdir DIR7\r"

expect "shell:/$ "
send "mkdir DIR8\r"

expect "shell:/$ "
send "mkdir DIR9\r"

expect "shell:/$ "
send "mkdir DIR10\r"

expect "shell:/$ "
send "mkdir DIR11\r"

expect "shell:/$ "
send "mkdir DIR12\r"

expect "shell:/$ "
send "mkdir DIR13\r"

expect "shell:/$ "
send "mkdir DIR14\r"

expect "shell:/$ "
send "mkdir DIR15\r"

expect "shell:/$ "
send "mkdir DIR16\r"

expect "shell:/$ "
send "mkdir DIR17FOO\r"

expect "shell:/$ "
send "ls\r"

expect "shell:/$ "
send "mkdir DIR18\r"

#expect "shell:/$ "
#send "mkdir DIR19\r"

#expect "shell:/$ "
#send "mkdir DIR20\r"


expect "shell:/$ "
send "ls\r"

expect "shell:/$ "
send "shutdown\r"

expect "*** System Shutdown ***\r"
send \001
send "x"
