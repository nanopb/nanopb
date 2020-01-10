all: index.html concepts.html reference.html security.html migration.html whats_new.html

%.html: %.rst
	rst2html --field-name-limit=32 --stylesheet=lsr.css --link-stylesheet $< $@
	sed -i 's!</head>!<link href="favicon.ico" type="image/x-icon" rel="shortcut icon" />\n</head>!' $@
