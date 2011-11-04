all: index.html concepts.html reference.html \
	generator_flow.png

%.png: %.svg
	rsvg $< $@

%.html: %.rst
	rst2html --stylesheet=lsr.css --link-stylesheet $< $@
