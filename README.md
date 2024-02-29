# LFV - Large file viewer
File editors often load the whole file into memory to display it. This approach does not work well with large text files, especially when your RAM is limited. This terminal-based file viewer lets you view large text files (up to 4GB) using O(1) memory. The file viewer also supports efficient text searching (O(pattern length + number of occurrences) memory).

<img width="1502" alt="Screen Shot 2024-02-17 at 7 41 59 am" src="https://github.com/hgv79116/large-file-viewer/assets/112223883/645a5149-1637-4797-b7a7-184b57a8d7f3">

## Opening the file viewer
After building the project, head to your terminal and run
```ansi
${executable} ${file_path}
```
The file path can be relative or absolute.

## How to use
The file viewer has two modes: _view_ mode (default) and _command_ mode. To turn on command mode, type "/". To go back to view mode, press Escape.

In view mode, you scroll up and down the document around your current position.

### Command mode
Once you are in command mode, you can enter the following commands
```ansi
/jump ${position}                          # Jumps to a position (in bytes from the start of the file)
/search ${pattern} -f ${from} -t ${to}     # Launch a search in the background for ${pattern} from ${from} to ${to}. The last two parameters are optional and default to the file's beginning and end.
/cancel                                    # Cancel the current search in the background if there is one.
/exit                                      # Exit the file viewer
```

Note: 
- Due to memory limit, the searches are currently limited to $5*10^6$ first occurrences of the pattern starting from ${from}
- To search for the previous/next matches, press **Shift+Tab** and **Tab**.
- You can iterate through matches in both modes.
