// Independent Foundation reference process; no Elytra or Qt code is linked.
import Foundation
let locale = Locale.current
let result: [String: Any] = [
    "system": Locale.canonicalLanguageIdentifier(from: locale.identifier),
    "country": locale.region?.identifier ?? "",
    "languages": Locale.preferredLanguages
]
let bytes = try JSONSerialization.data(withJSONObject: result, options: [.sortedKeys])
print(String(data: bytes, encoding: .utf8)!)
