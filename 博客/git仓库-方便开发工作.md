# git 仓库

在 `.12` 服务器中 `/data/panhy/code/OpenHarmony-v5.0-Beta1` 目录下创建了一个仓库，可安装扩展插件 `Git Graph` 进行管理，由于文件过多，请在对某个文件进行更改后提交更改到分支

### 创建 git 仓库

1. 打开终端，进入需要创建 git 仓库的目录。

2. 使用 `git init` 来初始化一个新的git仓库。 使用 `rm -rf .git` 删除仓库。

3. 使用 `git add [更改文件]` 将文件添加到暂存区

4. 使用 `git commit -m '提交的描述'` 将更改提交到仓库

   注意：在提交前需要设置你的用户名和邮箱地址

   ```
   git config –-global user.name “用户名”
   git config –-global user.email “邮箱地址”
   ```

### 常用 git 命令

1. 使用 `git status` 查看仓库状态。

2. 使用 `git log` 查看历史提交记录。

3. 使用 `git branch -a` 查看分支情况。

4. 使用 `git checkout [具体分支]` 切换到或创建某个分支。

5. 使用 `git revert [具体分支]` 回退版本

6. `git pull <URL>`

7. `git push -f <URL>` 强制推送

### 远程仓库

1. 如果您希望将本地仓库与远程仓库进行同步，您可以先在远程仓库中创建一个新的仓库，并获取远程仓库的URL。

   然后，使用以下命令将本地仓库与远程仓库关联：

   “`
   git remote add origin <远程仓库URL>
   “`

   替换`<远程仓库URL>`为您获取到的远程仓库URL。

2. 最后，使用以下命令将您的本地更改推送到远程仓库：

   “`
   git push origin master
   “`

   这将把本地仓库的更改推送到远程仓库的master分支上。
