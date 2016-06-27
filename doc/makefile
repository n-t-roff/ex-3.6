all: exrm.pdf exsummary.pdf ex3.1-3.5.pdf edittut.pdf viin.pdf viapp.pdf \
	visummary.pdf viapwh.pdf
# TROFF will frequently be just troff if you have a typesetter
# but no versatec.
TROFF=	troff
DPDF=	dpost | ps2pdf -
exrm.pdf: ex.rm
	${TROFF} -ms ex.rm | ${DPDF} $@
exsummary.pdf: ex.summary
	tbl ex.summary | ${TROFF} -ms | ${DPDF} $@
ex3.1-3.5.pdf: ex3.1-3.5
	#${TROFF} -ms ex1.1-2.0 | ${DPDF} $@
	#${TROFF} -ms ex2.0-3.1 | ${DPDF} $@
	${TROFF} -ms ex3.1-3.5 | ${DPDF} $@
edittut.pdf: edit.tut
	${TROFF} -ms edit.tut | ${DPDF} $@
# index for versatec is different from the one in edit.tut
# because the fonts are different and entries reference page
# rather than section numbers.  if you have a typesetter
# you should just use the index in edit.tut, and ignore editvindex.
editvindex: edit.vindex
	${TROFF} -ms -n22 edit.vindex | ${DPDF} $@
viin.pdf: vi.in
	tbl vi.in | ${TROFF} -ms | ${DPDF} $@
viapp.pdf: vi.chars
# numbers below are guessed at
# if using a typesetter use -n23 instead of -n25.
	tbl vi.chars | ${TROFF} -ms -n23 | ${DPDF} $@
visummary.pdf: vi.summary
	tbl vi.summary | ${TROFF} -ms | ${DPDF} $@
viapwh.pdf: vi.apwh.ms
	${TROFF} -ms vi.apwh.ms | ${DPDF} $@
clean:
	rm -f *.pdf
