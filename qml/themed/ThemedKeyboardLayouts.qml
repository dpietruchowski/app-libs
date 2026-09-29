import QtQml

QtObject {
    id: layouts

    readonly property var symbols: [
        ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"],
        ["@", "#", "&", "*", "-", "+", "=", "(", ")"],
        [".", ",", "?", "!", "'", "\"", ":"]
    ]

    readonly property var symbolAlternates: ({
        "-": ["–", "—"],
        "'": ["’"],
        "\"": ["“", "”"]
    })

    readonly property var qwerty: [
        ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"],
        ["a", "s", "d", "f", "g", "h", "j", "k", "l"],
        ["z", "x", "c", "v", "b", "n", "m"]
    ]

    readonly property string t9Punctuation: ".,?!'-\":"

    readonly property var t9Symbols: ["@#&", "*+=", "-_/", "()", ":;", "\"'", "%$€", "<>"]

    readonly property var t9Latin: ["abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz"]

    readonly property var fallback: ({
        letters: qwerty,
        alternates: symbolAlternates,
        t9: t9Latin,
        layerLabel: "ABC"
    })

    readonly property var languages: ({
        pl: {
            letters: qwerty,
            alternates: Object.assign({}, symbolAlternates, {
                "a": ["ą"],
                "c": ["ć"],
                "e": ["ę"],
                "l": ["ł"],
                "n": ["ń"],
                "o": ["ó"],
                "s": ["ś"],
                "z": ["ż", "ź"],
                "\"": ["„", "”"]
            }),
            t9: ["abcąć", "defę", "ghi", "jklł", "mnońó", "pqrsś", "tuv", "wxyzźż"],
            layerLabel: "ABC"
        },
        es: {
            letters: [
                ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"],
                ["a", "s", "d", "f", "g", "h", "j", "k", "l", "ñ"],
                ["z", "x", "c", "v", "b", "n", "m"]
            ],
            alternates: Object.assign({}, symbolAlternates, {
                "a": ["á"],
                "e": ["é"],
                "i": ["í"],
                "o": ["ó"],
                "u": ["ú", "ü"],
                "?": ["¿"],
                "!": ["¡"],
                "\"": ["«", "»"]
            }),
            t9: ["abcá", "defé", "ghií", "jkl", "mnoñó", "pqrs", "tuvúü", "wxyz"],
            layerLabel: "ABC"
        },
        fr: {
            letters: [
                ["a", "z", "e", "r", "t", "y", "u", "i", "o", "p"],
                ["q", "s", "d", "f", "g", "h", "j", "k", "l", "m"],
                ["w", "x", "c", "v", "b", "n"]
            ],
            alternates: Object.assign({}, symbolAlternates, {
                "a": ["à", "â", "æ"],
                "e": ["é", "è", "ê", "ë"],
                "i": ["î", "ï"],
                "o": ["ô", "œ"],
                "u": ["ù", "û", "ü"],
                "c": ["ç"],
                "y": ["ÿ"],
                "\"": ["«", "»"]
            }),
            t9: ["abcàâæç", "deféèêë", "ghiîï", "jkl", "mnoôœ", "pqrs", "tuvùûü", "wxyzÿ"],
            layerLabel: "ABC"
        },
        ru: {
            letters: [
                ["й", "ц", "у", "к", "е", "н", "г", "ш", "щ", "з", "х"],
                ["ф", "ы", "в", "а", "п", "р", "о", "л", "д", "ж", "э"],
                ["я", "ч", "с", "м", "и", "т", "ь", "б", "ю"]
            ],
            alternates: Object.assign({}, symbolAlternates, {
                "е": ["ё"],
                "ь": ["ъ"],
                "\"": ["«", "»"]
            }),
            t9: ["абвг", "деёжз", "ийкл", "мноп", "рсту", "фхцч", "шщъы", "ьэюя"],
            layerLabel: "АБВ"
        },
        uk: {
            letters: [
                ["й", "ц", "у", "к", "е", "н", "г", "ш", "щ", "з", "х", "ї"],
                ["ф", "і", "в", "а", "п", "р", "о", "л", "д", "ж", "є"],
                ["я", "ч", "с", "м", "и", "т", "ь", "б", "ю"]
            ],
            alternates: Object.assign({}, symbolAlternates, {
                "г": ["ґ"],
                "і": ["ї"],
                "\"": ["«", "»"]
            }),
            t9: ["абвгґ", "деєжз", "иіїйкл", "мноп", "рсту", "фхцч", "шщь", "юя"],
            layerLabel: "АБВ"
        }
    })

    function codeFor(language) {
        return (language ?? "").split(/[-_]/)[0].toLowerCase()
    }

    function layoutFor(language) {
        return layouts.languages[layouts.codeFor(language)] ?? layouts.fallback
    }
}
