SELECT table_name, table_id, column_count, tuple_count
FROM systables;

SELECT *
FROM syscolumns;

SELECT table_id, index_name, index_id, index_type, column_id, is_unique
FROM sysindexes;

SELECT *
FROM   angest AS a, mitarbeit AS m, project AS p
WHERE  p.projnr = m.projnr AND
       a.persnr = m.persnr;
