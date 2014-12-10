
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
send "cd DIR4\r"

expect "shell:/DIR4/$ "
send "cd ../\r"


expect "shell:/$ "
send "shutdown\r"

expect "*** System Shutdown ***\r"
send \001
send "x"
