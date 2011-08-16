all: index.html concepts.html reference.html

%.html: %.rst
	rst2html --stylesheet=lsr.css --link-stylesheet $< $@