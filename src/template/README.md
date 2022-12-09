# Templates to generate .H files

*You can safely ignore this directory unless you intend to make changes to the NDL headers themselves.*

Some of the headers included in NDL have repetitive code.
To minimize carpal tunnel syndrome, the repetitive bits are generated automatically by the [t4 processor](https://learn.microsoft.com/en-us/visualstudio/modeling/code-generation-and-t4-text-templates?view=vs-2022).
In a nutshell, t4 lets you intersperse the text with bits of C#, like for loops.

This project is meant to be used with [dotnet-t4](https://github.com/mono/t4), which you can easily install on your system by running

```
dotnet tool install -g dotnet-t4
```

Once you have that installed, you can regenerate the .H files from the .TT files in this directory by simply running:

```
build.cmd
```

