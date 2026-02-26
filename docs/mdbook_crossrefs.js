// Adds a few useful cross-referencing features to mdbook HTML output:
//
// - Automatically link identifiers inside code blocks to their reference sections, if found
// - Otherwise link to mdbook search
// - Add link to github search from mdbook search

function loadSearchIndexIfNeeded(callback) {
    if (window.search && window.search.doc_urls) {
        callback();
        return;
    }

    if (!window.path_to_searchindex_js) {
        console.warn("window.path_to_searchindex_js is not defined");
        return;
    }

    var script = document.createElement("script");
    script.src = window.path_to_searchindex_js;
    script.onload = function () {
        if (window.search && window.search.doc_urls) {
            callback();
        } else {
            console.warn("search.doc_urls still undefined after loading searchindex.js");
        }
    };
    document.head.appendChild(script);
}

function buildHeadingMap(docUrls) {
    var map = {};
    for (var i = 0; i < docUrls.length; i++) {
        var entry = docUrls[i]; // e.g. "foo.html#heading"
        var hashIndex = entry.indexOf("#");
        if (hashIndex !== -1 && hashIndex < entry.length - 1) {
            var heading = entry.substring(hashIndex + 1);
            map[heading] = entry;
        }
    }
    return map;
}

function findHeaderBackwards(startElement) {
    const walker = document.createTreeWalker(
        document.body,
        NodeFilter.SHOW_ELEMENT,
        {
            acceptNode(node) {
                return /^H[1-6]$/.test(node.tagName) ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_SKIP;
            }
        }
    );

    walker.currentNode = startElement;
    return walker.previousNode();
}

function linkifyCodeElements(headingMap) {
    var codeElements = document.querySelectorAll("code");

    codeElements.forEach(function (codeEl) {
        // Avoid re-processing if already modified
        if (codeEl.dataset.linkified === "true") return;

        var originalHTML = codeEl.innerHTML;

        // Replace whole words only
        var replacedHTML = originalHTML.replace(/\b([A-Za-z0-9_]+)(?!\.[hc])\b/g, function (match) {
            var lmatch = match.toLowerCase();
            if (headingMap.hasOwnProperty(lmatch)) {
                var prev_hdr = findHeaderBackwards(codeEl)
                if (prev_hdr.id != lmatch)
                {
                    var url = headingMap[lmatch];
                    return '<a href="' + url + '" title="API Reference for ' + match + '">' + match + '</a>';
                }
            }
            return match;
        });

        if (replacedHTML !== originalHTML) {
            codeEl.innerHTML = replacedHTML;
        }
        else
        {
            // Check if it is just a single identifier that we can add a search link for
            var text = originalHTML.trim();
            const isSingleWord = /^[a-zA-Z][a-zA-Z0-9_\(\)]*$/.test(text);
            const ignored = [
                'true', 'false', 'bool', 'uint32_t', 'int32_t', 'uint8_t', 'size_t',
                'float', 'double', '0', 'NULL'
            ];
            if (isSingleWord && !ignored.includes(text))
            {
                codeEl.innerHTML = '<a style="text-decoration: underline dotted; color: inherit" href="?search=' + text + '" title="Search for ' + text + '">' + text + '</a>';
            }
        }

        codeEl.dataset.linkified = "true";
    });
}

function addSearchGithubLink() {
    const searchInput = document.getElementById('mdbook-searchbar');
    const resultsOuter = document.getElementById('mdbook-searchresults-outer');

    if (!searchInput || !resultsOuter) return;

    ghLink = document.createElement('a');
    ghLink.id = 'github-search-link';
    ghLink.style = "float: right; margin-top: 1em;";
    ghLink.href = 'https://github.com/nanopb/nanopb/search';
    ghLink.textContent = 'Search on GitHub';
    resultsOuter.prepend(ghLink);

    function updateLink()
    {
        const query = searchInput.value.trim();
        let ghLink = document.getElementById('github-search-link');
        const encodedQuery = encodeURIComponent(query);
        ghLink.href = `https://github.com/nanopb/nanopb/search?q=${encodedQuery}`;
    }

    searchInput.addEventListener('input', updateLink);
    updateLink();
}

loadSearchIndexIfNeeded(function () {
    var headingMap = buildHeadingMap(window.search.doc_urls);
    linkifyCodeElements(headingMap);
    addSearchGithubLink();
});
