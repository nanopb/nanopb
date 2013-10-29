all: index.html concepts.html reference.html security.html \
	generator_flow.png

%.png: %.svg
	rsvg $< $@

%.html: %.rst
	rst2html --stylesheet=lsr.css --link-stylesheet $< $@
	sed -i 's!</head>!<link href="favicon.ico" type="image/x-icon" rel="shortcut icon" />\n</head>!' $@
