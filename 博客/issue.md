# tar 解压报错

`tar -zxvf link?target=https:%2F%2Frepo.huaweicloud.com%2Fopenharmony%2Fos%2F4.1-Release%2Fdayu200_standard_arm32.tar.gz`

由于压缩文件名中有冒号":"，它会被识别成一个地址而出现报错。此时需要使用 `–force-loca` 的参数来忽略冒号。

`tar -zxvf link?target=https:%2F%2Frepo.huaweicloud.com%2Fopenharmony%2Fos%2F4.1-Release%2Fdayu200_standard_arm32.tar.gz --force-local`

# 常用命令

1. tab 键自动补全

    在敲出命令的前几个字母的同时，按下 tab 键，系统会自动帮我们补全命令

2. history 游览历史

    当系统执行过一些命令后，可按 上下键 翻看以前的命令，history 将执行过的命令列举出来

    history保留了最近执行的命令记录，默认可以保留1000。
    历史清单从0开始编号到最大值。

    常见用法：
    ```
    history N		显示最近N条命令
    history -c		清除所有的历史记录
    history -w  xxx.txt	保存历史记录到文本xxx.txt
    ```

3. 命令行中的ctrl组合键

    ```
    Ctrl + c 结束正在运行的程序

    Ctrl + d 结束输入或退出shell

    Ctrl + s 暂停屏幕输出【锁住终端】

    Ctrl + q 恢复屏幕输出【解锁终端】

    Ctrl + l 清屏，【是字母L的小写】等同于Clear

    ctrl + a ：当前光标到行首

    ctrl + e ：当前光标到行尾

    ctrl + u ：删除当前光标到行首

    ctrl + k ：删除当前光标到行尾

    Ctrl + y 在光标处粘贴剪切的内容

    Ctrl + r 查找历史命令【输入关键字，就能调出以前执行过的命令】

    Ctrl + t 调换光标所在处与其之前字符位置，并把光标移到下个字符

    Ctrl + x + u 撤销操作

    Ctrl + z 转入后台运行
    ```
