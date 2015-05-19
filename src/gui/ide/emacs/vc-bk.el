;;; vc-bk.el --- support for BitKeeper version-control
;;
;; To use BitKeeper as a backend, add the following expression
;; to your .emacs file:
;;
;;     (let ((bin (replace-regexp-in-string "[\n\r]+$" ""
;;				     (shell-command-to-string "bk bin") nil)))
;;       (load-file (concat bin "/contrib/vc-bk.el"))
;;       (add-to-list 'vc-handled-backends 'Bk))
;;
;; (Assumes that BK is in your path)
;;
;; Alternatively, copy the vc-bk.el file to your emacs site-lisp
;; directory, but be advised it won't get updated automatically
;; when you upgrade BitKeeper.
;;
;; Author: Oscar Bonilla <ob@bitkeeper.com>
;;         Georg Nikodym <georgn@bitkeeper.com>

(eval-when-compile (require 'cl) (require 'vc))

;;; backend interfaces

(defun vc-bk-revision-granularity () 'repository)

(defun vc-bk-checkout-model (files)
  (let ((checkout (vc-bk-chomp
		   (shell-command-to-string "bk config checkout")))
	ret)
    (if (or (null checkout) (string-equal checkout ""))
	(progn
	  ;; assume default of checkout:edit
	  (setq checkout "edit")))
    (setq ret (cond
	       ((string-equal "get" checkout)
		(progn
		  (setq vc-keep-workfiles t)
		  'locking))
	       ((string-equal "none" checkout)
		(progn (setq vc-keep-workfiles nil)
		       'locking))
	       ((string-equal "last" checkout)
		(progn
		  (setq vc-keep-workfiles t)
		  'implicit))
	       ((string-equal "edit" checkout)
		(progn
		  (setq vc-keep-workfiles t)
		  'implicit))
	       nil))
    ;;(message "vc-bk-checkout-model: %s" ret)
    ret))

;; STATE-QUERYING FUNCTIONS
(defun vc-bk-registered (file)
  "Check whether a file is under BK control."
  (when (vc-bk-root file)
    (not (eq (vc-bk-state file) 'unregistered))))

(defun vc-bk-state (file)
  "BitKeeper specific version of `vc-state'."
  (let ((status (shell-command-to-string
		 (format "bk sfiles -vsixgcyp '%s'" (expand-file-name file)))))
    ;;; TODO: Many more status handling needed...
    ;;(message "%s" status)
    (if (or (null status) (string= status ""))
	'unregistered
      (progn
	(setq status (substring status 0 7))
	(cond
	 ((string-match "su-.-.." status) 'missing)	; clean (get it)
	 ((string-match "su-.G.." status) 'up-to-date)
	  ;; (if (vc-workfile-unchanged-p file)
	  ;;     'up-to-date
	  ;;   'unlocked-changes))  ; get
	 ((string-match "sl-.G.." status) 'up-to-date)	; edit, no diffs
	 ((string-match "slc.G.." status) 'edited)      ; diffs
	 ((string-match "x--.--." status) 'unregistered); extra
	 ((string-match "i--.--." status) 'ignored)	; ignored
	 ;; still missing, new files and deleted files...
	 nil)))))

(defun vc-bk-dir-state (dir)
  (with-temp-buffer
    (cd dir)
    (vc-bk-command (current-buffer) nil nil "sfiles" "-1" "-Eg")
    (goto-char (point-min))
    (let ((status-char nil)
	  (modifiedp nil)
	  (file nil))
      (setq n 1)
      (while (not (eobp))
	(setq status-char (char-after))
	(setq modifiedp (= (char-after (+ (point) 2)) ?c))
	(search-forward " ")
	(setq file
	      (expand-file-name (buffer-substring-no-properties
				(point) (line-end-position))))
	(cond
	 ((eq status-char ?s)
	  (if modifiedp
	      (vc-file-setprop file 'vc-state 'edited)
	    (vc-file-setprop file 'vc-state 'up-to-date)))
	 ((eq status-char ?x)
	  (vc-file-setprop file 'vc-backend 'none)
	  (vc-file-setprop file 'vc-state 'nil)))
;; 	(message "Line %d iteration %d: %s" (line-number-at-pos) n file)
	(setq n (1+ n))
	(forward-line)
	(line-beginning-position)))))

(defun vc-bk-working-revision (file)
  "BitKeeper specific version of `vc-working-revision'."
  (shell-command-to-string
   (format "bk log -r+ -d:REV: '%s'" file)))

(defun vc-bk-workfile-version (file)
  "BitKeeper specific version of `vc-workfile-version'."
  (vc-bk-working-revision file))

(defun vc-bk-find-version (file rev buffer)
  (vc-bk-command buffer 0 file "get" "-qp" (format "-r%s" (if rev rev "+"))))

(defun vc-bk-find-revision (file revision buffer)
  (vc-bk-find-version file revision buffer))

(defun vc-bk-diff (files &optional rev1 rev2 buffer)
  (let ((buf (or buffer "*vc-diff*")))
    (if (and rev1 rev2)
	(vc-bk-command buf 0 files "diffs" "-up"
		       (format "-r%s" rev1)
		       (format "-r%s" rev2))
      (vc-bk-command buf 0 files "diffs" "-up"
		     (format "-r%s" (or rev1 "+"))))))

(defun vc-bk-print-log (files buffer &optional shortlog start-revision limit)
  "Get change log associated with FILES."
  (vc-setup-buffer buffer)
  (let ((inhibit-read-only t))
    (with-current-buffer
	buffer
      (apply 'vc-bk-command buffer 0 files
	     (nconc (if (string-equal (substring (car files) -1 nil) "/")
			(list "changes" "-S")
		      (list "log"))
		    (list (format "-%d" (if limit limit 0))))))))



(defun vc-bk-diff-tree (dir &optional rev1 rev2)
  (vc-bk-diff dir rev1 rev2))

;; TODO: implement this function using bk diffs
(defun vc-bk-workfile-unchanged-p (file)
   "See if the workfile is changed"
   ;;(message "vc-bk-workfile-unchanged-p")
   nil)

;; STATE-CHANGING FUNCTIONS

(defun vc-bk-create-repo ()
  "Create a new BitKeeper repository."
  (vc-bk-command nil 0 nil "setup" "-efc/dev/null" "."))

(defun vc-bk-register (files &optional rev comment)
  "Register FILE into BitKeeper."
  (vc-bk-command nil 0 files "new" (format "-y%s" comment)))

;; XXX: someday I'll do a citool in elisp :)
(defun vc-bk-checkin (files rev comment)
  (vc-bk-command nil 0 files "ci" (format "-y%s" comment)))

;; this is sometimes called on an already bk-edited file, so we need
;; to ignore the exit code
(defun vc-bk-checkout (file &optional editable rev)
  (if editable
      (vc-bk-command nil nil file "edit" "-qS")
    (vc-bk-command nil nil file "get" "-qS")))

(defalias 'vc-bk-responsible-p 'vc-bk-root)

(defun vc-bk-root (file)
  (or (vc-find-root file ".bk/BitKeeper/etc")
      (vc-find-root file "BitKeeper/etc")))

(defun vc-bk-chomp (str)
  (replace-regexp-in-string "[\n\r]+$" "" str nil))

(defun vc-bk-command (buffer okstatus file-or-list &rest flags)
  "Wrap `vc-do-command'"
  (apply 'vc-do-command (or buffer "*vc*") okstatus "bk" file-or-list flags))

(defun vc-bk-annotate-command (file buf &optional rev)
  "Annotate a BitKeeper file"
    (vc-bk-command buf 0 file "annotate" (if rev (concat "-r" rev))))

(defun vc-bk-mode-line-string (file)
  "Return string for placement into the modeline for FILE."
    (vc-default-mode-line-string 'Bk file))

(defun vc-bk-revert (file &optional contents-done)
  "Revert FILE to the version it was based on. If FILE is a directory,
revert all subfiles."
  (if (file-directory-p file)
      (mapc 'vc-sccs-revert (vc-expand-dirs (list file)))
    (vc-bk-command nil 0 file "unedit")
    (let ((editable (eq (vc-bk-checkout-model buffer-file-name) 'implicit)))
      (vc-checkout buffer-file-name editable)
      (vc-state-refresh buffer-file-name 'bk)
      (vc-mode-line buffer-file-name)
      (setq buffer-read-only (not editable)))
    (vc-file-setprop file 'vc-working-revision nil)))

(defun vc-bk-find-file-hook ()
  "If you're loading an unchecked out file, then get it first"
  ;;(message "vc-bk-find-file-hook: %s" buffer-file-name)
  (when (vc-bk-root buffer-file-name)
    (let ((state (vc-bk-state buffer-file-name)))
      (if (eq state 'missing)
	  ;; we have a cleaned bk file (not checked out)
	  ;;  - get it
	  ;;  - put the file contents into the buffer
	  ;;    (necessary because this hook happens after
	  ;;    the file has been loaded)
	  ;;  - update vc's state cache
	  ;;  - fix the modeline (else you end up with Bk?1.x)
	  ;;  - make the buffer read-only
	  ;;
	  ;; Unless, we have checkout:last, in which case we
	  ;; check out for edit
	  (let ((editable (eq (vc-bk-checkout-model buffer-file-name) 'implicit)))
	    (vc-checkout buffer-file-name editable)
	    (vc-state-refresh buffer-file-name 'bk)
	    (vc-mode-line buffer-file-name)
	    (setq buffer-read-only (not editable)))))))

(provide 'vc-bk)
;;;; vc-bk.el ends here
