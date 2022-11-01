serve:
	mkdocs serve -a localhost:9090

serve-once:
	mkdocs serve -a localhost:9090 --no-livereload

debug:
	mkdocs serve -v -a localhost:9090

build-site: clean-site
	mkdocs build

clean-site:
	rm -rf site

serve-static: build-site
	python3 -m http.server --directory site

gh-deploy:
	mkdocs gh-deploy --force