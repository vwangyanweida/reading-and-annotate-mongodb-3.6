# reading-and-annotate-mongodb-3.6.1
mongodb-3.6.1源码注释分析，持续更新
  
### .为什么需要对mongodb开源版本做二次开发，需要做哪些必备二次开发:  
===================================  
* [为何需要对开源mongodb社区版本做二次开发，需要做哪些必备二次开发](https://github.com/y123456yz/reading-and-annotate-mongodb-3.6.1/blob/master/development_mongodb.md)  
* [对开源mongodb社区版本做二次开发收益列表](https://my.oschina.net/u/4087916/blog/3063529)  

### .mongodb性能优化、采坑、问题定位解决等:   
  * [百万级高并发mongodb集群性能数十倍提升优化实践(上篇)](https://my.oschina.net/u/4087916/blog/3141909)      
  * [百万级高并发mongodb集群性能数十倍提升优化实践(下篇)](https://my.oschina.net/u/4087916/blog/3155205)    
  

说明:  
===================================  
MongoDB是一个基于分布式文件存储的数据库。由C++语言编写。旨在为WEB应用提供可扩展的高性能数据存储解决方案。  
是一个介于关系数据库和非关系数据库之间的产品，是非关系数据库当中功能最丰富，最像关系数据库的。他支持的数据结构非常松散，是类似json的bson格式，因此可以存储比较复杂的数据类型。Mongo最大的特点是他支持的查询语言非常强大，其语法有点类似于面向对象的查询语言，几乎可以实现类似关系数据库单表查询的绝大部分功能，而且还支持对数据建立索引。  

mongodb存储引擎wiredtiger源码分析  
===================================  
https://github.com/y123456yz/reading-and-annotate-wiredtiger-3.0.0   
  
  
rocksdb存储引擎源码分析  
===================================  
https://github.com/y123456yz/reading-and-annotate-rocksdb-6.1.2   
  
  
源码中文注释
===================================   

