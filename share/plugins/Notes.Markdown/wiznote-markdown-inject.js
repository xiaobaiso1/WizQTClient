
function ParseMarkdownContent(objHtmDoc, MarkdownFunction) {
    try {
        $(objHtmDoc).find('img').each(function(index, img) {
            var src = $(img).attr('src');
            $(img).after('<span>![' + src + '](' + src + ')</span>');
        });
    } catch (e) {
        alert(e);
    }
    try {
        $(objHtmDoc).find('a').each(function(index, link) {
            var href = $(link).attr('href');
            if (href.indexOf("wiz:") == 0) {
                var text = $(link).text();
                $(link).after('<span>[' + text + '](' + href + ')</span>');
                $(link).remove();
            }
        });
    } catch (e) {
        alert(e);
    }

    var body = objHtmDoc.getElementsByTagName('BODY').item(0);
    var bodyText = body.innerText.replace(/ \\\\ /ig, ' \\\\\\\\ ').replace(/(\r\n){2}/g, "\r\n")
    var parsedHtml = MarkdownFunction(bodyText, {
        breaks: true,
        langPrefix: 'language-',
        highlight: function(code, language) {
            if (language) {
                return hljs.highlight(language, code).value
            } else {
                return hljs.highlightAuto(code).value
            }
        }
    });
    body.innerHTML = (parsedHtml);
}

function initMarkdown() {
    if (jQuery) {
        ParseMarkdownContent(document, marked);
    } else {
        setTimeout(initMarkdown, 100);
    }
}
initMarkdown();