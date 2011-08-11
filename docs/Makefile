all: index.html encoding.html

%.html: %.rst
	rst2html --stylesheet=lsr.css --link-stylesheet $< $@