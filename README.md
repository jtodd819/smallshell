Small Shell by James Todd

Compilation : 'gcc -std=c99 -o smallsh smallsh.c'
Running : './smallsh'

Usage: 'command [arg1 arg2 ...] [< input_file] [> output_file] [&]'

Syntax:
	1)'&' symbol used as the last word of command line to run the command in the background.
	2)'<' symbol used to redirect STDIN to the file name 'input_file' for the given command.
	3)'>' symbol used to redirect STDOUT to the file name 'output_file' for the given command.
	4)Command lines support up to 2048 characters and 512 arguments.
	5)'#' at the beginning of a command line is used as a comment and causes nothing to be executed.
	6)'$$' is expanded by default to the process id of the running small shell instance.
	7)SIGINT (Ctrl-c) kills the currently running foreground process, if running, and prints out the signal number
	which killed the process.
	8)SIGSTP (Ctrl-z) switches to foreground only mode (background commands with '&' are now only run in foreground).
	Hit Ctrl-z again to switch back to background/foreground mode.
	9)'exit' closes the small shell.
	10)'cd' changes directories to the HOME directory if no arguments specified or to a given directory name if specified.
	11)'status' prints the exit status / terminating signal of the last foreground process.

