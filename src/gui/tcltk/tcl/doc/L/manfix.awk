BEGIN { 
        skip = 0
	zero = 0
}

$0 ~ /.SH "POD ERRORS"/ {
        skip = 1
}

{
	if (skip == 0) {
		print
		zero++
	}
}

END {
	if (zero > 0) {
		printf ".\\\" help://L\n" 
		printf ".\\\" help://l\n" 
	}
}
