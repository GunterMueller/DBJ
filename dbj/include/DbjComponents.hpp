/*************************************************************************\
 *                                                                       *
 * (C) 2004-2005                                                         *
 * Lehrstuhl fuer Datenbanken und Informationssysteme                    *
 * Friedrich-Schiller-Universitaet Jena                                  *
 * Ernst-Abbe-Platz 1-2                                                  *
 * 07745 Jena                                                            *
 *                                                                       *
\*************************************************************************/

#if !defined(__DbjComponents_hpp__)
#define __DbjComponents_hpp__


/** Systemkomponenten.
 *
 * Alle Komponenten des Systems.  Dieses "enum" wird hauptsaechlich von der
 * Fehlerbehandlung verwendet, um nachzuvollziehen welche Komponente einen
 * Fehler erkannt und gemeldet hat.
 */
enum DbjComponent {
    /// die Kommandozeile steuert alle Anfragen und Anweisungen)
    CommandLine,
    /// der Compiler uebersetzt Anweisungen in internen Zugriffsplan
    Compiler,
    /// der Optimizer analysiert und optimiert den internen Zugriffsplan
    Optimizer,
    /// der Katalog verwaltet alle Tabellen, Spalten und Indexe im System
    CatalogManager,
    /// die Laufzeitumgebung fuehrt den optimierten Zugriffsplan aus
    RunTime,
    /// der Record Manager verwaltet die Records auf den Datenseiten
    RecordManager,
    /// der Index Manager verwaltet B-Baum und Hash Indexe
    IndexManager,
    /// der Lock Manager verwaltet die Sperren auf einzelnen Daten- oder
    /// Indexseiten
    LockManager,
    /// der Buffer Manager stellt den Zugriff auf Daten- oder Indexseiten zur
    /// Verfuegung
    BufferManager,
    /// der File Manager laedt Seiten von Platte
    FileManager,
    /// verschiedene Unterstuetzungsfunktionen (Tracing, Fehlerbehandlung,
    /// Speicherverwaltung, ...)
    Support
};

#endif /* __DbjComponents_hpp__ */

