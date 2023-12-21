# WiFi-2-Serial Connector

Dieses kleine Projekt orientiert sich im Kern an dem ESP8266 Beispiel [WiFiTelnetToSerial](https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi/examples/WiFiTelnetToSerial), ergänzt allerdings eine Reihe von Konfigurationsmöglichkeiten und auch ein bisschen Sicherheit.

# Erstinstallation

Nach dem Einspielen der Firmware auf einem ESP8266 signalisieren die LEDs folgenden Zustand:

- grün: aus
- rot: aus
- blau: an, es gibt keine WiFi Verbindung

### WiFi

Als erster Schritt muss die WiFi Verbindung über WPS konfiguriert werden. Dazu wird der Menütaster einmal gedrückt - durch weiteres zweimaligs Drücken des Tasters wird die Konfiguration beenden.

- grün: an, Konfiguration aktiv
- rot: aus
- blau: an, bereit für eine WiFi Anmeldung über WPS

Wird nun der Bestätigungstaster gedrückt, so beginnt die WPS Anmeldung. Schlägt diese fehl, so blinkt die blaue LED einige Male und es kann ein neuer Versuch gestartet werden. Ist die Anmeldung erfolgreich, so geht die grüne LED aus und signalisiert das Beenden der Konfiguration. Die blaue LED geht aus, sobald eine erfolgreiche Anmeldung im WLAN durchgeführt wurde.

**Tipp**: ist die blaue LED an so bedeutet das immer, dass keine Netzwerkverbindung vorliegt - oder man befindet sich in der WPS Konfiguration.

**Tipp**: die WPS Konfiguration kann jederzeit wiederholt werden, um eine Anmeldung in einem anderen Netzwerk vorzunehmen. Die letzte erfolgreiche Anmeldung wird vermerkt und nach dem Neustart direkt verwendet.

### Serielle Verbindung und Sicherheit

Nach der erfolgreichen Anmeldung zeigen die LEDs den neuen Betriebszustand:

- grün: an, Konfiguration unvollständig
- rot: aus
- blau: aus

In diesem Zustand kann der erste TCP Client die Konfiguration vorgeben. Er meldet sich dazu auf dem TCP/IP Port **29111** an und sendet eine Konfigurationsinformation defr folgenden Art:

`PASSWORD;9600;8;2;0<Carriage Return>`

- PASSWORD ist das im Betrieb zu verwendende Kennwort, es kann relativ frei vergeben werden - relativ frei heißt zumindest das Semikolon und das CR sind verboten
- Die Baudrate der angeschlossenen seriellen Leitung beträgt 9600
- Es werden 8 Datenbits verwendet - andere Werte sind aktuell nicht erlaubt
- Es werden zwei Stoppbits verwendet - alternativ nur eines
- Es findet keine Paritätsprüfung statt - aktuell wird nur diese Einstellung unterstützt

Sobald eine Konfiguration übertragen wurde wird diese vermerkt und das Gerät neu gestartet. Ist die grüne LED dann immer noch an, dann war die Konfigurationzeichenkette ungültig und es muss eine korrekte übermittelt werden.

Das Gerät sollte sich nun im normalen Betriebsmodus befinden und alle LEDs sind aus.

**Tipp**: die Konfiguration kann jederzeit erneuert werden, zum Beispiel weil das Kennwort in falsche Hände gefallen ist. In diesem Fall wird der Menütaster zweimal betätigt, die LEDs zeigen dann:

- grün: an, Konfigurationsmodus aktiv
- rot: an, Konfiguration kann zurückgesetzt werden
- blau: aus

Durch den Bestätigungstaster wird die Konfiguration zurückgesetzt und das Gerät neu gestartet. Die Konfiguration kann dann wir beschrieben über den ersten TCP/IP Client vorgenommen werden.

# Betrieb

Nach dem Starten sind alle LEDs aus. Verbindet sich ein TCP/IP Client über Port **29111** so geht die rote LED an. Ein Client autorisiert sich einmalig am Anfang der Sitzung durch die Übertragung des in der Konfiguration hinterlegten Kennworts.

`PASSWORD<Carriage Return>`

Danach werden alle gesendeten Bytes unverändert an die serielle Schnittstelle übertragen und ebenso alle Daten von dieser an den Client übermittelt.

Es kann sich immer nur ein Client anmelden. Wird der Aufbau einer weiteren TCP/IP Verbindung erkannt, so wird diese direkt wieder terminiert.

# Einige Bemerkungen zum Abschluss

Das mit der Sicherheit ist natürlich nur etwas halbherzig. Das Kennwort wird immer im Klartext übertragen, so dass jemand mit Zugriff auf das WLAN diese auslesen könnte. Für den normalen Betrieb in einem normal gesicherten WLAN sollte das aber kein Problem sein.

In der Implementierung sind noch einige Punkte unvollständig, der geneigte Entwickler möge sich hier austoben. Insbesondere kritisch sind:

- es wird nicht erkannt, wenn die WLAN Verbindung zusammen bricht - was ja durchaus nicht unüblich ist. Weder wird versucht, diese neu erzustellen, noch wird der TCP/IP Server neu gestartet.
- es gibt keine Zwischenspeicherung von Daten - in keine Richtung. Zu schneller Dateneingang von dem seriellen Anschluss oder der TCP/IP Verbindung kann zu Datenverlust führen. Das zu verifizieren ist aber etwas müßig und nicht erfolgt.
