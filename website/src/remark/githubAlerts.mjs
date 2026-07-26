// Превращает GitHub-алерты в admonition Docusaurus.
//
//   > [!WARNING]
//   > текст
//
// GitHub рисует такой блок сам, Docusaurus про него не знает и оставляет
// «[!WARNING]» текстом. Плагин переписывает узел в containerDirective, который
// штатный admonition-плагин Docusaurus разбирает как ::: блок - поэтому он и
// подключается через beforeDefaultRemarkPlugins.
//
// Одна разметка работает и в репозитории, и на сайте.

const TYPES = {
	NOTE: 'note',
	TIP: 'tip',
	IMPORTANT: 'info',
	WARNING: 'warning',
	CAUTION: 'danger',
};

const MARKER = /^\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\][ \t]*\n?/;

function convert(node) {
	if (node.type !== 'blockquote' || !node.children?.length)
		return null;

	const first = node.children[0];
	if (first.type !== 'paragraph' || !first.children?.length)
		return null;

	const text = first.children[0];
	if (text.type !== 'text')
		return null;

	const match = MARKER.exec(text.value);
	if (!match)
		return null;

	text.value = text.value.slice(match[0].length);

	// Маркер стоял на своей строке: параграф начинается с пустого текста.
	if (!text.value)
		first.children.shift();
	if (!first.children.length)
		node.children.shift();

	return {
		type: 'containerDirective',
		name: TYPES[match[1]],
		attributes: {},
		data: {},
		children: node.children,
	};
}

function walk(node) {
	if (!Array.isArray(node.children))
		return;

	for (let i = 0; i < node.children.length; i++) {
		const converted = convert(node.children[i]);
		if (converted)
			node.children[i] = converted;

		walk(node.children[i]);
	}
}

export default function githubAlerts() {
	return (tree) => walk(tree);
}
