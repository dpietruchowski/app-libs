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

    readonly property var fallback: ({
        letters: qwerty,
        alternates: symbolAlternates,
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
            layerLabel: "АБВ"
        }
    })

    function layoutFor(language) {
        var code = (language ?? "").split(/[-_]/)[0].toLowerCase()
        return layouts.languages[code] ?? layouts.fallback
    }
}
