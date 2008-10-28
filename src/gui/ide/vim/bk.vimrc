"" BitKeeper VIM mode 
""  Copyright (c) 2001 by Aaron Kushner; All rights reserved.
""
"" %W%
""
"" Instructions:
""   Save this file in your ~/.vim directory and then add the following
""   line to your .vimrc:  so ~/.vimr/bk.vimrc
""
"" Notes:
""   You should be running Vim 5.5 or later and have syntax coloring turned
""   on to make the most out of this mode.
""
""   For the unfortunate emacs users, bitkeeper vc-mode extensions are on
""   there way. (20FEB2001)
""            
""
"" bk macro commands:
""
"" ;ba    Shows annotated listing of the current file
"" ;bp    Shows prs output for the current in the top window
"" ;bm    Shows prs output for the ChangeSet in the top window
"" ;bP    Pull from the parent repository
"" ;bU    Push to the parent repository
"" ;bd    Shows diff output in the upper window. The diff output is between 
""           the currently edited version and the most recent version
"" ;bD    Brings up the gui difftool 
"" ;bs    Shows status of the repository
"" ;br    Shows the gui revsion tool
"" ;bH    Shows the gui helptool tool
"" ;;     Unload the current buffer. This macro does not unload non-bk mode
""           windows. Since it does a forced-unload, didn't want people to
""           lose work. Probably should do a normal unload if not in one of
""           the mode windows so the user can have a chance to save work
""
"" NOT WORKING YET (Give feedback and suggestions, but don't even think
"" of complaining about these yet)
""
"" ;bc    not done
"" ;bC    not done
"" ;bn    Revision control current file and check it back out for editting
""
"" TODO: The annotated listing should have an option for drilling down to
""       a specific rev when clicking return on the rev number. Also, should
""	have option to display what annotated columns the user wants to see.

let rev = "%W%"
if rev == "%W%"
    let rev = "1.4Pre"
endif

" Need to unlet this while doing development
unlet! bk_macros_loaded

echohl WarningMsg | echon "Loading bk_macros v" . rev . ".... " | echohl None
if version >= 505 && !exists("bk_macros_loaded")
    echon "succeeded"
    let bk_macros_loaded = 1

    au BufEnter ci-window call CiEnter()
    au BufLeave ci-window call CiLeave()
    au BufDelete ci-window call CiDelete()
    au BufEnter comments-window call CommentEnter()
    au BufLeave comments-window call CommentLeave()
    au BufDelete comments-window call CommentDelete()

    fu! CiEnter()
	set hidden
	noremap <CR> :call CiFile()<CR>
	inoremap <CR> <Esc>:call CiFile()<CR>
	nmap  ;;  :call ExitCitool()<CR>
    endfun

    fu! CiDelete()
    	map ;bz :call Sfiles()
    endfun

    fu! CiLeave()
	let cpo_save = &cpo
	let &cpo = ""
	if mapcheck("<CR>") != ""
	    unmap <CR>
	    iunmap <CR>
    	endif
	nmap  ;;  :bd!<CR><CR>
        let &cpo = cpo_save
    endfun

    fu! ExitCitool()
	bu 1
	if bufexists("diff-window")
	    bd! diff-window
	endif
	if bufexists("comments-window")
	    bd! comments-window
	endif
	if bufexists("ci-window")
	    bd! ci-window
	endif
	resize
    endfun

    fu! CiFile()
	let lnum = line(".")
	let line = getline(lnum)

        let file = substitute(line, '\S*\s*\(\S*\).*', '\1', "")
	echon "Checking in: " . file
	return
	call Diffs(file)
    endfun

    fu! CheckinFile(file)
    	if !bufexists("comments-window")
	    new comments-window
	    resize 10
	else 
	    buffer comments-window
	    resize 10
	endif
    endfun

    fu! Sfiles()
	let lfile = bufname("%")
	call Diffs(lfile)
	only
    	if !bufexists("comments-window")
	    new comments-window
	    resize 10
	else 
	    buffer comments-window
	    resize 10
	endif
    	if !bufexists("ci-window")
	    new ci-window
	else 
	    buffer ci-window
	endif
       	exe "r!bk -R sfiles -gvc"
        set readonly
        set nomodified
	set nowrap
	resize 10
	goto 1
    endfun
    command! -nargs=+ -complete=command Sfiles call Sfiles(<q-args>)

    fu! PrsSyntax()
	let b:current_syntax = "prs"
	syn region BkPrsD start="^[a-zA-Z]" end="$"
	syn region BkPrsT start="TAG" end="$"
	syn match BkPrsP  "^\s*P"
	highlight link BkPrsD Comment
	highlight link BkPrsP Comment
	highlight link BkPrsT Error
    endfun
    
    fu! SyncSyntax()
	let b:current_syntax = "sync"
	syn region BkSyncA start="^[a-zA-Z]" end="$"
	syn region BkSyncB start="TAG" end="$"
	syn match BkSyncC  "^\s*P"
	highlight link BkSyncA Comment
	highlight link BkSyncB Comment
	highlight link BkSyncC Error
    endfun

    fu! AnnotateSyntax()
	let b:current_syntax = "annotate"
	syn match BkRev "[1-9]\.[0-9][0-9\.]*"
 	"syn match BkRev "[1-9]\.[0-9]" contained
	syn region BkAnnotate start="^" end="|" contains=BkRev
	highlight link BkAnnotate Comment
	highlight link BkRev Statement
    endfun

    fu! OpenIfNew( name )
	" Check if the buffer was already around, and then deleting and 
	" re-loading it, if it was.
        " Find out if we already have a buffer for it
        let buf_no = bufnr(expand(a:name))
        " If there is a diffs.tx buffer, delete it
        if buf_no > 0
	    exe 'bd! '.a:name
        endif
        " (Re)open the file (update).
	exe ':sp '.a:name
    endfun
    
    "" Only force-unload the directories we know that the user
    "" won't lose work.
    "" OK, this is getting silly. How about setting a local buffer variable
    "" that lets Unloading succeed?
    fu! Unload()
    	let buf = bufname("%")
    	if buf == "comments-window" || buf == "annotate-window" || buf == "ci-window" || buf == "diff-window" || buf == "prs-window"
	    bd!
	endif
    endfun

    " TODO: Ask the user for a specific revision
    fu! Diffs(file)
    	if !bufexists("diff-window")
	    new diff-window
	else
	    sb diff-window
	endif
        exe "r!bk diffs " . a:file
	goto 1
	set syntax=on
	set filetype=diff
	set readonly
        set nomodified
	"set syntax=diff
    endfun
    command! -nargs=+ -complete=command Diffs call Diffs(<q-args>)

    fu! Status()
	let lfile = bufname("%")
        new
        exe "r!bk status -v"
	set readonly
        set nomodified
	set filetype=status
	set syntax=status
	goto 1
    endfun
    command! -nargs=+ -complete=command Status call Status(<q-args>)

    fu! Revtool()
	let lfile = bufname("%")
	let choice = confirm("revtool on?", "&File\n&Project\n&Cancel", 1)
	if choice == 0
	   echo "Make up your mind!"
	elseif choice == 1
        	exe "!bk revtool " . lfile . "&"
	elseif choice == 2
        	exe "!bk revtool &"
	elseif choice == 3
		return
	endif
    endfun
    command! -nargs=+ -complete=command Status call Status(<q-args>)

    " Show the PRS output for a file or cset
    fu! Prs(file)
        let dspec = "-d\':DPN:@:I:, :Dy:-:Dm:-:Dd: :T::TZ:, :P:\$if(:HT:){@:HT:}\\n\$each(:C:){  (:C:)\\n}\$each(:SYMBOL:){  TAG: (:SYMBOL:)\\n}\\n\' "
	if a:file == "ChangeSet"
        	new prs-window
        	exe "r!bk -R prs " . "-hn " . dspec . 'ChangeSet'
	else 
		let lfile = bufname("%")
        	new prs-window
        	exe "r!bk prs " . "-hn " . dspec . lfile
	endif
	call PrsSyntax()
        set filetype=prs
	set syntax=prs
        set readonly
        set nomodified
	goto 1
    endfun
    command! -nargs=+ -complete=command Prs call Prs(<q-args>)

    " Show the annotate listing for a file
    " TODO: Need option to deselect m u d or use a specific rev
    fu! Annotate()
	let lfile = bufname("%")
       	new annotate-window
       	exe "r!bk annotate -amud " lfile
	call AnnotateSyntax()
        set filetype=annotate
	set syntax=annotate
        set readonly
        set nomodified
	set nowrap
	goto 1
    endfun
    command! -nargs=+ -complete=command Annotate call Annotate(<q-args>)


    fu! Sync(command)
	let lfile = bufname("%")
	if a:command == "Push"
	    let choice = confirm("Type of Push?", "&Real\n&Null\n&Cancel", 1)
	    let op = "push"
	else
	    let choice = confirm("Type of Pull?", "&Real\n&Null\n&Cancel", 1)
	    let op = "pull"
	endif
	if choice == 0
	   echo "Make up your mind!"
	elseif choice == 1
        	exe "new"
        	exe "r!bk " . op
	elseif choice == 2
        	exe "new"
        	exe "r!bk " . op . " -nl"
		goto 1
	elseif choice == 3
		return
	endif
	call SyncSyntax()
        set filetype=sync
	set syntax=sync
        set readonly
        set nomodified
	resize 10
    endfun
    command! -nargs=+ -complete=command Sync call Sync(<q-args>)

    fu! DiffTool(command)
        let tmp = tempname()
        exe "!bk prs " . argv(0) . ">" . tmp
        exe "sv " . tmp
    endfun
    command! -nargs=+ -complete=command DiffTool call DiffTool(<q-args>)

    if has("gui_running")
	    nmenu B&K.&Add<Tab>;CN :!bk new %<CR>
	    "use this one if you want a window to popup
	    nmenu B&K.&Checkin<Tab>;CI :!bk ci -l -y"`gprompt -l Log:`" %<CR>
	    nmenu B&K.&Commit<Tab>;CC :echo "enter commit comments:" <bar> !bk commit -m "$<" %
	    nmenu B&K.&Diff<Tab>;CD :!bk difftool %<CR>
	    nmenu B&K.&History<Tab>;CH :!bk revtool %<CR>
	    nmenu B&K.&Log<Tab>;Cprs :!bk prs %<CR>
	    nmenu B&K.&Status<Tab>;CS :!bk status -v<CR>
	    nmenu B&K.&Pull<Tab>;CP :!bk pull<CR>
	    nmenu B&K.&Push<Tab>;Cu :!bk push<CR>
    endif

    nmap  ;bn :!bk new %; bk edit %<CR>
    "use this one if you want a window to popup
    "nmap  ;bc :!bk ci -ly"`gprompt -l Checkin Comments:`" %;<CR>
    "nmap  ;bc :echo "enter checkin comment: " <bar> !bk ci -l -y"$<" %
    "nmap  ;bC :echo "enter log string: " <bar> !bk commit -m "$<" %
    nmap  ;bc :CheckinFile(bufname("%"))<CR><CR>
    nmap  ;bD :!bk difftool % &<CR><CR>
    nmap  ;bd :call Diffs(bufname("%"))<CR><CR>
    nmap  ;br :call Revtool() <CR><CR>
    nmap  ;bH :!bk helptool &<CR><CR>
    nmap  ;bp :Prs % <CR><CR>
    nmap  ;ba :call Annotate() <CR><CR>
    nmap  ;bm :Prs ChangeSet<CR><CR>
    nmap  ;bs :call Status() <CR><CR>
    nmap  ;bP :call Sync("Pull") <CR><CR>
    nmap  ;bU :call Sync("Push") <CR><CR>
    nmap  ;;  :call Unload() <CR><CR>
    nmap  ;'  :bd! <CR><CR>
    "testing the ;bt feature for now
    nmap  ;bt :call Sfiles() <CR><CR>
else
    echon "failed"
    if version < 505 
    	echon " Requires Vim > v5.5 "
    endif
    if exists("bk_macros_loaded")
    	echon " Macros alread loaded"
    endif
endif
