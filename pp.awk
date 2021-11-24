# SPDX-License-Identifier: MIT


# This is similar to the C preprocessor, but very /very/ bad:
#   * macros expand with macroname{arg1, arg2}, because it doesn't break syntax highlighting, and
#   * macros end definition with #endefine instead of using line continuations, which plays better with syntax highlighting.


BEGIN {
	dir = ARGV[1]
	sub(/[^\/]+$/, "", dir)

	for (i = 2; i < ARGC; ++i) {
		eq = index(ARGV[i], "=")
		v = substr(ARGV[i], eq + 1)
		gsub(/\\n/, "\n", v)
		macro_contents[substr(ARGV[i], 1, eq - 1)] = v
	}

	incfile = ""
}


function input() {
	if(NF == 2 && $1 == "#include") {
		if(incfile != "") {
			print "Nested include! (" incfile " -> " $2 ")" >> "/dev/stderr"
			exit 1
		}

		gsub(/"/, "", $2)
		incfile = dir $2

		while((getline < incfile) == 1)
			input()
		incfile = ""
	} else if(NF >= 2 && $1 == "#define") {
		split($2, nameargs, "(")
		macroname = nameargs[1]

		gsub(/[\(,]/, "", nameargs[2])
		if(nameargs[2] != ")") {
			last = nameargs[2] ~ /\)$/
			sub(/\)/, "", nameargs[2])
			macro_args[macroname,1] = nameargs[2]

			for(i = 3; !last; ++i) {
				last = $i ~ /\)$/

				sub(/[,\)]/, "", $i)
				macro_args[macroname,i - 1] = $i
			}
		}

		while(1) {
			if(incfile == "")
				getline
			else
				getline < incfile

			if($0 == "#endefine")
				break

			macro_contents[macroname] = macro_contents[macroname] $0 "\n"
		}
	} else {
		for(macroname in macro_contents) {
			if(pos = index($0, macroname "{")) {
				epos = pos + index(substr($0, pos), "}")

				pref = substr($0, 1, pos - 1)
				postf = substr($0, epos)

				arg_str = substr($0, pos + length(macroname) + 1, epos - (pos + length(macroname) + 1) - 1)
				split(arg_str, args, /,[[:space:]]/)
				body = macro_contents[macroname]
				for(i in args) {
					gsub(/\\/, "\\\\", args[i])
					gsub(/&/, "\\\\&", args[i])
					gsub(macro_args[macroname,i], args[i], body)
				}

				$0 = pref body postf
			}
		}
		print
	}
}


{
	input()
}
