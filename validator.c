const char *HTTPValidateMethodLength = "Methods should have at least one character.";
const char *HTTPValidateMethodTChar = "Methods should only consist of tchar's";

static int IsAlpha(char c) {
	return (c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A);
}

static int IsDigit(char c) {
	return c >= 0x30 && c <= 0x39;
}

static int IsTChar(char c) {
	return IsAlpha(c)  || IsDigit(c) ||
			 c == '!'  || c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
			 c == '\'' || c == '*' || c == '+' || c == '-' || c == '.' || c == '^' ||
			 c == '_'  || c == '`'  || c == '|' || c == '~';
}

const char *ValidateMethod(const char *method) {
	size_t length = strlen(method);

	if (length == 0)
		return HTTPValidateMethodLength;

	size_t i;
	for (i = 0; i < length; i++) {
		if (!IsTChar(method[0]))
			return HTTPValidateMethodTChar;
	}

	return NULL;
}
 
