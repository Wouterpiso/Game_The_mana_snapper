# Use Case Document voor The ManaSnapper

## Use Case Diagram.

![no image](../Media/UseCaseDiagramTheManaSnapper.drawio.png)

## Use Case Beschrijvingen

| UC01 | Collectie Bekijken |
|---|---|
| Beschrijving | Het systeem toont de opgeslagen kaarten in de collectie. |
| Pre-condities | Het systeem kan de collectie openen. |
| Post-condities | De kaarten uit de collectie zijn weergegeven en het systeem keert terug naar het hoofdmenu. |
| Scenario | 1. Het systeem opent de collectie. <br> 2. Het systeem haalt de opgeslagen kaarten op. <br> 3. Het systeem geeft de kaarten weer. <br> 4. Het systeem wacht totdat een kaart wordt geselecteerd of totdat terugkeren naar het hoofdmenu (back button op scherm) wordt gekozen. <br> 5. Het systeem toont de geselecteerde kaart in meer detail en wacht totdat de terug nop word geraakt (X button op scherm). <br> 6. Het systeem keert terug naar de collection menu. |
| Uitzonderingen | - Als de collectie geen kaarten bevat, toont het systeem een melding dat de collectie leeg is en keert het systeem terug naar het hoofdmenu. <br> - Als de collectie niet kan worden ingelezen, toont het systeem een foutmelding en keert het systeem terug naar het hoofdmenu. |

| UC02 | Kaart Toevoegen |
|---|---|
| Beschrijving | Het systeem voegt een kaart aan de collectie toe. |
| Pre-condities | Het systeem kan de collectie aanpassen. |
| Post-condities | De gescande of gekozen kaart is toegevoegd en het systeem keert terug naar het hoofdmenu. |
| Scenario | 1. Het systeem wacht totdat een kaart wordt opgegeven. <br> 2. Het systeem identificeert de kaart naam. <br> 3. Het systeem haalt de kaartgegevens op. <br> 4. Het systeem toont de opgehaalde kaartgegevens. <br> 5. Het systeem wacht op bevestiging om de kaart toe te voegen of op de keuze om terug te keren naar het hoofdmenu. <br> 6. Het systeem voegt de kaart aan de collectie toe. <br> 7. Het systeem slaat de bijgewerkte collectie op. <br> 8. Het systeem keert terug naar het hoofdmenu. |
| Uitzonderingen | - Als het systeem geen kaart kan identificeren, toont het systeem een melding dat de kaart niet herkend is en keert het systeem terug naar het hoofdmenu. <br> - Als het systeem de kaartgegevens niet kan ophalen, toont het systeem een melding dat de kaart niet gevonden is en keert het systeem terug naar het hoofdmenu. <br> - Als het toevoegen wordt geannuleerd, voegt het systeem de kaart niet toe en keert het systeem terug naar het hoofdmenu. |

| UC03 | Kaart Verwijderen |
|---|---|
| Beschrijving | Het systeem verwijdert een kaart uit de collectie. |
| Pre-condities | Het systeem toont de collectie. |
| Post-condities | De geselecteerde kaart is verwijderd en het systeem keert terug naar het hoofdmenu. |
| Scenario | 1. Het systeem toont de kaarten uit de collectie. <br> 2. Het systeem wacht totdat een kaart wordt geselecteerd of totdat terugkeren naar het hoofdmenu wordt gekozen. <br> 3. Het systeem toont de geselecteerde kaart met een back en een verwijder button. <br> 4. Het systeem vraagt om bevestiging voor het verwijderen. <br> 5. Het systeem wacht op bevestiging. <br> 6. Het systeem verwijdert de geselecteerde kaart uit de collectie. <br> 7. Het systeem slaat de bijgewerkte collectie op. <br> 8. Het systeem toont de bijgewerkte collectie. <br> 9. Het systeem keert terug naar het hoofdmenu. |
| Uitzonderingen | - Als het verwijderen wordt geannuleerd, laat het systeem de kaart in de collectie staan en keert het systeem terug naar het hoofdmenu. <br> - Als de geselecteerde kaart niet meer in de collectie staat, toont het systeem een foutmelding, blijft de collectie ongewijzigd en keert het systeem terug naar het hoofdmenu. |

| UC04 | Kaartgegevens Ophalen |
|---|---|
| Beschrijving | Het systeem haalt de gegevens van een kaart op uit Scryfall. |
| Pre-condities | Het systeem heeft een kaart geïdentificeerd en beschikt over de benodigde kaartinformatie. |
| Post-condities | De gegevens van de kaart zijn ontvangen en beschikbaar om aan de collectie toe te voegen. Daarna keert het systeem terug naar het hoofdmenu of naar de aanroepende use case. |
| Scenario | 1. Het systeem ontvangt de kaart naam. <br> 2. Het systeem zoekt de kaart op bij Scryfall. <br> 3. Het systeem ontvangt de kaartgegevens. <br> 4. Het systeem stelt de kaartgegevens beschikbaar aan de aanroepende use case. <br> 5. Na afronding of beëindiging keert het systeem terug naar het hoofdmenu. |
| Uitzonderingen | - Als Scryfall geen kaart met de opgegeven gegevens vindt, toont het systeem een melding dat de kaart niet gevonden is en keert het systeem terug naar het hoofdmenu. <br> - Als Scryfall niet bereikbaar is, toont het systeem een melding dat de kaartgegevens niet opgehaald kunnen worden en keert het systeem terug naar het hoofdmenu. <br> - Als de ontvangen kaartgegevens ongeldig zijn, voegt het systeem de gegevens niet toe aan de collectie en keert het systeem terug naar het hoofdmenu. |

| UC05 | Collectie Importeren |
|---|---|
| Beschrijving | Het systeem importeert een bestaande collectie vanaf de computer. |
| Pre-condities | Het systeem kan een collectiebestand openen. |
| Post-condities | De kaarten uit het geïmporteerde bestand zijn opgeslagen in de collectie en het systeem keert terug naar het hoofdmenu. |
| Scenario | 1. Het systeem wacht totdat een collectiebestand wordt geselecteerd of totdat terugkeren naar het hoofdmenu wordt gekozen. <br> 2. Het systeem leest het geselecteerde bestand in. <br> 3. Het systeem controleert de inhoud van het bestand. <br> 4. Het systeem voegt de kaarten uit het bestand toe aan de collectie. <br> 5. Het systeem slaat de bijgewerkte collectie op. <br> 6. Het systeem meldt dat de collectie succesvol is geïmporteerd. <br> 7. Het systeem keert terug naar het hoofdmenu. |
| Uitzonderingen | - Als geen bestand wordt geselecteerd, annuleert het systeem de import en keert het systeem terug naar het hoofdmenu. <br> - Als het bestand niet kan worden gelezen, toont het systeem een foutmelding, laat het systeem de bestaande collectie ongewijzigd en keert het systeem terug naar het hoofdmenu. <br> - Als het bestand geen geldig collectieformaat bevat, toont het systeem een foutmelding, past het systeem de collectie niet aan en keert het systeem terug naar het hoofdmenu. |

| UC06 | Collectie Exporteren |
|---|---|
| Beschrijving | Het systeem exporteert de opgeslagen collectie naar de computer. |
| Pre-condities | Het systeem beschikt over een collectie die kan worden geëxporteerd. |
| Post-condities | De collectie is opgeslagen in een bestand op de computer en het systeem keert terug naar het hoofdmenu. |
| Scenario | 1. Het systeem haalt de collectie op. <br> 2. Het systeem wacht totdat een locatie en bestandsnaam worden opgegeven of totdat terugkeren naar het hoofdmenu wordt gekozen. <br> 3. Het systeem zet de collectie om naar het gekozen bestandsformaat. <br> 4. Het systeem slaat het bestand op de computer op. <br> 5. Het systeem meldt dat de collectie succesvol is geëxporteerd. <br> 6. Het systeem keert terug naar het hoofdmenu. |
| Uitzonderingen | - Als geen locatie wordt geselecteerd, annuleert het systeem de export en keert het systeem terug naar het hoofdmenu. <br> - Als het bestand niet kan worden opgeslagen, toont het systeem een foutmelding en keert het systeem terug naar het hoofdmenu. <br> - Als de collectie leeg is, meldt het systeem dat er geen kaarten zijn om te exporteren en keert het systeem terug naar het hoofdmenu. |
