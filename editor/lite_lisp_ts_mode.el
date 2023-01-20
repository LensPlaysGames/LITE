;;; lite_lisp_ts_mode.el --- Syntax highlighting for LITE LISP based on the tree-sitter grammar -*- lexical-binding: t -*-

;; Author: Rylan Lens Kellogg
;; Maintainer: Rylan Lens Kellogg
;; Version: 0.0.1
;; Keywords   : LITE LISP languages tree-sitter


;; This file is not part of GNU Emacs

;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.


;;; Commentary:

;; commentary

;;; Code:

(require 'treesit)

(declare-function treesit-parser-create "treesit.c")
;(declare-function treesit-induce-sparse-tree "treesit.c")
;(declare-function treesit-node-parent "treesit.c")
;(declare-function treesit-node-start "treesit.c")
;(declare-function treesit-node-end "treesit.c")
;(declare-function treesit-node-child "treesit.c")
;(declare-function treesit-node-child-by-field-name "treesit.c")
;(declare-function treesit-node-type "treesit.c")


(defvar lite-lisp-ts-mode--keywords
  '("if" "when" "unless" "let" "define" "set" "defun" "defmacro" "macro"
    ;; "while"
    )
  "LITE LISP keywords for tree-sitter font-locking.")

(defvar lite-lisp-ts-mode--operators
  '("'" "`" "," ",@"
    )
  "LITE LISP operators for tree-sitter font-locking.")

(defvar lite-lisp-ts-mode--delimiters
  '("(" ")")
  "LITE LISP delimiters for tree-sitter font-locking.")

(defun lite-lisp-ts-mode--font-lock-settings ()
  "Tree-sitter font-lock settings."
  (treesit-font-lock-rules
   :language 'lite_lisp
   :override t
   :feature 'comment
   `((comment) @font-lock-comment-face)

   :language 'lite_lisp
   :override t
   :feature 'string
   `((string) @font-lock-string-face)

   :language 'lite_lisp
   :override t
   :feature 'number
   `((number) @font-lock-number-face)

   :language 'lite_lisp
   :override t
   :feature 'keyword
   `([,@lite-lisp-ts-mode--keywords] @font-lock-keyword-face)

   :language 'lite_lisp
   :feature 'operator
   `([,@lite-lisp-ts-mode--operators] @font-lock-operator-face)

   :language 'lite_lisp
   :feature 'delimiter
   `([,@lite-lisp-ts-mode--delimiters] @font-lock-delimiter-face)
   ))

;;;###autoload
(define-derived-mode lite-lisp-ts-mode lisp-mode
  "Major mode for editing LITE LISP, powered by tree-sitter."
  :group 'lite

  ;; Electric
  (setq-local electric-indent-chars
              (append "()" electric-indent-chars))

  (setq-local treesit-font-lock-feature-list
              '(( keyword comment )
                ( string )
                ( number operator )
                ( delimiter )))

  (unless (treesit-ready-p 'lite_lisp)
    (error "Tree-sitter for LITE LISP isn't available"))

  (treesit-parser-create 'lite_lisp)

  ;; Comments.
  (setq-local comment-start ";;")
  (setq-local comment-end   "")

  ;; TODO: Maybe actually write a tree sitter indent function.
  ;; Use `C-h f treesit-simple-indent RET`
  (setq-local treesit-simple-indent-rules nil)

  ;; Font-lock.
  (setq-local treesit-font-lock-settings
              (lite-lisp-ts-mode--font-lock-settings))

  (add-to-list 'auto-mode-alist '("\\.lt\\'" . lite-lisp-ts-mode))

  (treesit-major-mode-setup))


(provide 'lite-lisp-ts-mode)

;;; lite_lisp_ts_mode.el ends here
