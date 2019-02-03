-- .memcpy on
-- .parallel off
-- .codegen print off
-- .timings on
-- .gentests off
-- .gentests path test-tmp
-- .gentests no-execute
-- .memcpy off
.parallel on
.cpuonly off
--off
.hybrid off
.codegen print off
.timings off
.timings csv off
.gentests off
.gentests path test-tmp
.gentests no-execute
.echo results off

-- warm up scala and python
.warmup 
select sum(lo_revenue) as lo_revenue, d_year, p_brand1
from ssbm_lineorder, ssbm_date, ssbm_part, ssbm_supplier
where lo_orderdate = d_datekey
 and lo_partkey = p_partkey
 and lo_suppkey = s_suppkey
 and p_category = 'MFGR#12'
 and s_region = 'AMERICA' 
group by d_year, p_brand1 ;

.timings on
.timings csv on
.create test ssb_q1_1_% from 
select sum(lo_extendedprice*lo_discount) as revenue
from ssbm_lineorder, ssbm_date
where lo_orderdate = d_datekey
 and d_year = 1993
 and lo_discount between 1 and 3
 and lo_quantity < 25 ;

.create test ssb_q1_2_% from 
select sum(lo_extendedprice*lo_discount) as revenue
from ssbm_lineorder, ssbm_date
where lo_orderdate = d_datekey
 and d_yearmonthnum = 199401
 and lo_discount between 4 and 6
 and lo_quantity between 26 and 35 ;

.create test ssb_q1_3_% from 
select sum(lo_extendedprice*lo_discount) as revenue
from ssbm_lineorder, ssbm_date
where lo_orderdate = d_datekey
 and d_weeknuminyear = 6
 and d_year = 1994
 and lo_discount between 5 and 7
 and lo_quantity between 26 and 35 ;

-- Q2
.create test ssb_q2_1_% from 
select sum(lo_revenue), d_year, p_brand1
from ssbm_lineorder, ssbm_part, ssbm_supplier, ssbm_date
 where lo_orderdate = d_datekey
 and lo_partkey = p_partkey
 and lo_suppkey = s_suppkey
 and p_category = 'MFGR#12'
 and s_region = 'AMERICA'
group by d_year, p_brand1
order by d_year, p_brand1 ; 

.create test ssb_q2_2_% from 
select sum(lo_revenue) as lo_revenue, d_year, p_brand1
from ssbm_lineorder, ssbm_date, ssbm_part, ssbm_supplier
where lo_orderdate = d_datekey
 and lo_partkey = p_partkey
 and lo_suppkey = s_suppkey
 and p_brand1 between 'MFGR#2221' and 'MFGR#2228'
 and s_region = 'ASIA' 
group by d_year, p_brand1
order by d_year, p_brand1 ;

.create test ssb_q2_3_% from 
select sum(lo_revenue) as lo_revenue, d_year, p_brand1
from ssbm_lineorder, ssbm_date, ssbm_part, ssbm_supplier
where lo_orderdate = d_datekey
 and lo_partkey = p_partkey
 and lo_suppkey = s_suppkey
 and p_brand1 = 'MFGR#2239'
 and s_region = 'EUROPE' 
group by d_year, p_brand1
order by d_year, p_brand1 ;

-- Q3
.create test ssb_q3_1_% from 
select c_nation, s_nation, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and c_region = 'ASIA'
 and s_region = 'ASIA'
 and d_year >= 1992
 and d_year <= 1997 
group by c_nation, s_nation, d_year
order by d_year asc, lo_revenue desc ;

.create test ssb_q3_2_% from 
select c_city, s_city, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and c_nation = 'UNITED STATES'
 and s_nation = 'UNITED STATES'
 and d_year >= 1992
 and d_year <= 1997 
group by c_city, s_city, d_year
order by d_year asc, lo_revenue desc ;

.create test ssb_q3_3_% from 
select c_city, s_city, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and (c_city='UNITED KI1' or c_city='UNITED KI5')
 and (s_city='UNITED KI1' or s_city='UNITED KI5')
 and d_year >= 1992
 and d_year <= 1997 
group by c_city, s_city, d_year
order by d_year asc, lo_revenue desc ;

.create test ssb_q3_4_% from 
select c_city, s_city, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and (c_city='UNITED KI1' or c_city='UNITED KI5')
 and (s_city='UNITED KI1' or s_city='UNITED KI5')
 and d_yearmonth = 'Dec1997' 
group by c_city, s_city, d_year
order by d_year asc, lo_revenue desc ;

-- Q4
.create test ssb_q4_1_% from 
select d_year, c_nation, sum(lo_revenue - lo_supplycost) as profit
from ssbm_date, ssbm_customer, ssbm_supplier, ssbm_part, ssbm_lineorder
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_partkey = p_partkey
 and lo_orderdate = d_datekey
 and c_region = 'AMERICA'
 and s_region = 'AMERICA'
 and (p_mfgr = 'MFGR#1' or p_mfgr = 'MFGR#2') 
group by d_year, c_nation
order by d_year, c_nation ;

.create test ssb_q4_2_% from 
select d_year, s_nation, p_category, sum(lo_revenue - lo_supplycost) as profit
from ssbm_date, ssbm_customer, ssbm_supplier, ssbm_part, ssbm_lineorder
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_partkey = p_partkey
 and lo_orderdate = d_datekey
 and c_region = 'AMERICA'
 and s_region = 'AMERICA'
 and (d_year = 1997 or d_year = 1998)
 and (p_mfgr = 'MFGR#1' or p_mfgr = 'MFGR#2') 
group by d_year, s_nation, p_category
order by d_year, s_nation, p_category ;

.create test ssb_q4_3_% from 
select d_year, s_city, p_brand1, sum(lo_revenue - lo_supplycost) as profit
from ssbm_date, ssbm_customer, ssbm_supplier, ssbm_part, ssbm_lineorder
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_partkey = p_partkey
 and lo_orderdate = d_datekey
 and c_region = 'AMERICA'
 and s_nation = 'UNITED STATES'
 and (d_year = 1997 or d_year = 1998)
 and p_category = 'MFGR#14' 
group by d_year, s_city, p_brand1
order by d_year, s_city, p_brand1 ;




.parallel on
.cpuonly on
.hybrid off




.create test ssb_q1_1_% from 
select sum(lo_extendedprice*lo_discount) as revenue
from ssbm_lineorder, ssbm_date
where lo_orderdate = d_datekey
 and d_year = 1993
 and lo_discount between 1 and 3
 and lo_quantity < 25 ;

.create test ssb_q1_2_% from 
select sum(lo_extendedprice*lo_discount) as revenue
from ssbm_lineorder, ssbm_date
where lo_orderdate = d_datekey
 and d_yearmonthnum = 199401
 and lo_discount between 4 and 6
 and lo_quantity between 26 and 35 ;

.create test ssb_q1_3_% from 
select sum(lo_extendedprice*lo_discount) as revenue
from ssbm_lineorder, ssbm_date
where lo_orderdate = d_datekey
 and d_weeknuminyear = 6
 and d_year = 1994
 and lo_discount between 5 and 7
 and lo_quantity between 26 and 35 ;

-- Q2
.create test ssb_q2_1_% from 
select sum(lo_revenue), d_year, p_brand1
from ssbm_lineorder, ssbm_part, ssbm_supplier, ssbm_date
 where lo_orderdate = d_datekey
 and lo_partkey = p_partkey
 and lo_suppkey = s_suppkey
 and p_category = 'MFGR#12'
 and s_region = 'AMERICA'
group by d_year, p_brand1
order by d_year, p_brand1 ; 

.create test ssb_q2_2_% from 
select sum(lo_revenue) as lo_revenue, d_year, p_brand1
from ssbm_lineorder, ssbm_date, ssbm_part, ssbm_supplier
where lo_orderdate = d_datekey
 and lo_partkey = p_partkey
 and lo_suppkey = s_suppkey
 and p_brand1 between 'MFGR#2221' and 'MFGR#2228'
 and s_region = 'ASIA' 
group by d_year, p_brand1
order by d_year, p_brand1 ;

.create test ssb_q2_3_% from 
select sum(lo_revenue) as lo_revenue, d_year, p_brand1
from ssbm_lineorder, ssbm_date, ssbm_part, ssbm_supplier
where lo_orderdate = d_datekey
 and lo_partkey = p_partkey
 and lo_suppkey = s_suppkey
 and p_brand1 = 'MFGR#2239'
 and s_region = 'EUROPE' 
group by d_year, p_brand1
order by d_year, p_brand1 ;

-- Q3
.create test ssb_q3_1_% from 
select c_nation, s_nation, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and c_region = 'ASIA'
 and s_region = 'ASIA'
 and d_year >= 1992
 and d_year <= 1997 
group by c_nation, s_nation, d_year
order by d_year asc, lo_revenue desc ;

.create test ssb_q3_2_% from 
select c_city, s_city, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and c_nation = 'UNITED STATES'
 and s_nation = 'UNITED STATES'
 and d_year >= 1992
 and d_year <= 1997 
group by c_city, s_city, d_year
order by d_year asc, lo_revenue desc ;

.create test ssb_q3_3_% from 
select c_city, s_city, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and (c_city='UNITED KI1' or c_city='UNITED KI5')
 and (s_city='UNITED KI1' or s_city='UNITED KI5')
 and d_year >= 1992
 and d_year <= 1997 
group by c_city, s_city, d_year
order by d_year asc, lo_revenue desc ;

.create test ssb_q3_4_% from 
select c_city, s_city, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and (c_city='UNITED KI1' or c_city='UNITED KI5')
 and (s_city='UNITED KI1' or s_city='UNITED KI5')
 and d_yearmonth = 'Dec1997' 
group by c_city, s_city, d_year
order by d_year asc, lo_revenue desc ;

-- Q4
.create test ssb_q4_1_% from 
select d_year, c_nation, sum(lo_revenue - lo_supplycost) as profit
from ssbm_date, ssbm_customer, ssbm_supplier, ssbm_part, ssbm_lineorder
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_partkey = p_partkey
 and lo_orderdate = d_datekey
 and c_region = 'AMERICA'
 and s_region = 'AMERICA'
 and (p_mfgr = 'MFGR#1' or p_mfgr = 'MFGR#2') 
group by d_year, c_nation
order by d_year, c_nation ;

.create test ssb_q4_2_% from 
select d_year, s_nation, p_category, sum(lo_revenue - lo_supplycost) as profit
from ssbm_date, ssbm_customer, ssbm_supplier, ssbm_part, ssbm_lineorder
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_partkey = p_partkey
 and lo_orderdate = d_datekey
 and c_region = 'AMERICA'
 and s_region = 'AMERICA'
 and (d_year = 1997 or d_year = 1998)
 and (p_mfgr = 'MFGR#1' or p_mfgr = 'MFGR#2') 
group by d_year, s_nation, p_category
order by d_year, s_nation, p_category ;

.create test ssb_q4_3_% from 
select d_year, s_city, p_brand1, sum(lo_revenue - lo_supplycost) as profit
from ssbm_date, ssbm_customer, ssbm_supplier, ssbm_part, ssbm_lineorder
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_partkey = p_partkey
 and lo_orderdate = d_datekey
 and c_region = 'AMERICA'
 and s_nation = 'UNITED STATES'
 and (d_year = 1997 or d_year = 1998)
 and p_category = 'MFGR#14' 
group by d_year, s_city, p_brand1
order by d_year, s_city, p_brand1 ;




.parallel on
.cpuonly off
.hybrid on




.create test ssb_q1_1_% from 
select sum(lo_extendedprice*lo_discount) as revenue
from ssbm_lineorder, ssbm_date
where lo_orderdate = d_datekey
 and d_year = 1993
 and lo_discount between 1 and 3
 and lo_quantity < 25 ;

.create test ssb_q1_2_% from 
select sum(lo_extendedprice*lo_discount) as revenue
from ssbm_lineorder, ssbm_date
where lo_orderdate = d_datekey
 and d_yearmonthnum = 199401
 and lo_discount between 4 and 6
 and lo_quantity between 26 and 35 ;

.create test ssb_q1_3_% from 
select sum(lo_extendedprice*lo_discount) as revenue
from ssbm_lineorder, ssbm_date
where lo_orderdate = d_datekey
 and d_weeknuminyear = 6
 and d_year = 1994
 and lo_discount between 5 and 7
 and lo_quantity between 26 and 35 ;

-- Q2
.create test ssb_q2_1_% from 
select sum(lo_revenue), d_year, p_brand1
from ssbm_lineorder, ssbm_part, ssbm_supplier, ssbm_date
 where lo_orderdate = d_datekey
 and lo_partkey = p_partkey
 and lo_suppkey = s_suppkey
 and p_category = 'MFGR#12'
 and s_region = 'AMERICA'
group by d_year, p_brand1
order by d_year, p_brand1 ; 

.create test ssb_q2_2_% from 
select sum(lo_revenue) as lo_revenue, d_year, p_brand1
from ssbm_lineorder, ssbm_date, ssbm_part, ssbm_supplier
where lo_orderdate = d_datekey
 and lo_partkey = p_partkey
 and lo_suppkey = s_suppkey
 and p_brand1 between 'MFGR#2221' and 'MFGR#2228'
 and s_region = 'ASIA' 
group by d_year, p_brand1
order by d_year, p_brand1 ;

.create test ssb_q2_3_% from 
select sum(lo_revenue) as lo_revenue, d_year, p_brand1
from ssbm_lineorder, ssbm_date, ssbm_part, ssbm_supplier
where lo_orderdate = d_datekey
 and lo_partkey = p_partkey
 and lo_suppkey = s_suppkey
 and p_brand1 = 'MFGR#2239'
 and s_region = 'EUROPE' 
group by d_year, p_brand1
order by d_year, p_brand1 ;

-- Q3
.create test ssb_q3_1_% from 
select c_nation, s_nation, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and c_region = 'ASIA'
 and s_region = 'ASIA'
 and d_year >= 1992
 and d_year <= 1997 
group by c_nation, s_nation, d_year
order by d_year asc, lo_revenue desc ;

.create test ssb_q3_2_% from 
select c_city, s_city, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and c_nation = 'UNITED STATES'
 and s_nation = 'UNITED STATES'
 and d_year >= 1992
 and d_year <= 1997 
group by c_city, s_city, d_year
order by d_year asc, lo_revenue desc ;

.create test ssb_q3_3_% from 
select c_city, s_city, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and (c_city='UNITED KI1' or c_city='UNITED KI5')
 and (s_city='UNITED KI1' or s_city='UNITED KI5')
 and d_year >= 1992
 and d_year <= 1997 
group by c_city, s_city, d_year
order by d_year asc, lo_revenue desc ;

.create test ssb_q3_4_% from 
select c_city, s_city, d_year, sum(lo_revenue) as lo_revenue
from ssbm_customer, ssbm_lineorder, ssbm_supplier, ssbm_date
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_orderdate = d_datekey
 and (c_city='UNITED KI1' or c_city='UNITED KI5')
 and (s_city='UNITED KI1' or s_city='UNITED KI5')
 and d_yearmonth = 'Dec1997' 
group by c_city, s_city, d_year
order by d_year asc, lo_revenue desc ;

-- Q4
.create test ssb_q4_1_% from 
select d_year, c_nation, sum(lo_revenue - lo_supplycost) as profit
from ssbm_date, ssbm_customer, ssbm_supplier, ssbm_part, ssbm_lineorder
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_partkey = p_partkey
 and lo_orderdate = d_datekey
 and c_region = 'AMERICA'
 and s_region = 'AMERICA'
 and (p_mfgr = 'MFGR#1' or p_mfgr = 'MFGR#2') 
group by d_year, c_nation
order by d_year, c_nation ;

.create test ssb_q4_2_% from 
select d_year, s_nation, p_category, sum(lo_revenue - lo_supplycost) as profit
from ssbm_date, ssbm_customer, ssbm_supplier, ssbm_part, ssbm_lineorder
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_partkey = p_partkey
 and lo_orderdate = d_datekey
 and c_region = 'AMERICA'
 and s_region = 'AMERICA'
 and (d_year = 1997 or d_year = 1998)
 and (p_mfgr = 'MFGR#1' or p_mfgr = 'MFGR#2') 
group by d_year, s_nation, p_category
order by d_year, s_nation, p_category ;

.create test ssb_q4_3_% from 
select d_year, s_city, p_brand1, sum(lo_revenue - lo_supplycost) as profit
from ssbm_date, ssbm_customer, ssbm_supplier, ssbm_part, ssbm_lineorder
where lo_custkey = c_custkey
 and lo_suppkey = s_suppkey
 and lo_partkey = p_partkey
 and lo_orderdate = d_datekey
 and c_region = 'AMERICA'
 and s_nation = 'UNITED STATES'
 and (d_year = 1997 or d_year = 1998)
 and p_category = 'MFGR#14' 
group by d_year, s_city, p_brand1
order by d_year, s_city, p_brand1 ;

quit
