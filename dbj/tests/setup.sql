CREATE TABLE angest (
   persNr    INTEGER NOT NULL,
   vorname   VARCHAR(10),
   nachname  VARCHAR(20) NOT NULL,
   gehalt    INTEGER NOT NULL,
   adresse   VARCHAR(20),

   PRIMARY KEY(persNr)
);
CREATE INDEX angest_nachname ON angest(nachname);


CREATE TABLE project (
   projNr        INTEGER NOT NULL,
   name          VARCHAR(20) NOT NULL,
   prioritaet    INTEGER,
   beschreibung  VARCHAR(1000),

   PRIMARY KEY(projNr)
);
CREATE INDEX project_prio ON project(prioritaet);

CREATE TABLE mitarbeit (
   persNr    INTEGER NOT NULL,
   projNr   INTEGER NOT NULL,
   percent  INTEGER
);
CREATE INDEX mitarbeit_persNr ON mitarbeit(persNr);
CREATE INDEX mitarbeit_projNr ON mitarbeit(projNr);

COMMIT WORK;

INSERT INTO angest
VALUES (1, 'Klaus', 'Kuespert', 10000, NULL),
       (2, 'Knut', 'Stolze', 1500, 'Ernst-Abbe-Platz 2'),
       (3, 'Thomas', 'Mueller', 3000, 'Ernst-Abbe-Platz 2');
INSERT INTO angest
VALUES (4, 'Hannes', 'Moser', 500, 'Ernst-Abbe-Platz 1');

INSERT INTO project
VALUES (1, 'DBS-Entwicklung', 1, NULL),
       (2, 'DBS1', 5, NULL),
       (3, 'XML-Seminar', 99, NULL);

INSERT INTO mitarbeit VALUES (1, 1, 10), (1, 2, 30), (1, 3, 2);
INSERT INTO mitarbeit VALUES (2, 1, 90);
INSERT INTO mitarbeit VALUES (3, 1, 0), (3, 3, 30);
INSERT INTO mitarbeit VALUES (4, 1, 100);

COMMIT;
