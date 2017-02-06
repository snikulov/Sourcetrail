#include "data/PersistentStorage.h"

#include <sstream>
#include <queue>

#include "utility/Cache.h"
#include "utility/file/FileSystem.h"
#include "utility/logging/logging.h"
#include "utility/messaging/type/MessageNewErrors.h"
#include "utility/messaging/type/MessageStatus.h"
#include "utility/text/TextAccess.h"
#include "utility/TimePoint.h"
#include "utility/tracing.h"
#include "utility/utility.h"
#include "utility/utilityString.h"
#include "utility/Version.h"
#include "utility/utilityString.h"

#include "data/graph/token_component/TokenComponentAggregation.h"
#include "data/graph/token_component/TokenComponentFilePath.h"
#include "data/graph/token_component/TokenComponentSignature.h"
#include "data/graph/Graph.h"
#include "data/location/TokenLocation.h"
#include "data/location/TokenLocationFile.h"
#include "data/location/TokenLocationLine.h"
#include "data/parser/ParseLocation.h"

PersistentStorage::PersistentStorage(const FilePath& dbPath)
	: m_sqliteStorage(dbPath)
{
	m_commandIndex.addNode(0, SearchMatch::getCommandName(SearchMatch::COMMAND_ALL));
	m_commandIndex.addNode(0, SearchMatch::getCommandName(SearchMatch::COMMAND_ERROR));
	// m_commandIndex.addNode(0, SearchMatch::getCommandName(SearchMatch::COMMAND_COLOR_SCHEME_TEST));
	m_commandIndex.finishSetup();
}

PersistentStorage::~PersistentStorage()
{
}

Id PersistentStorage::addNode(int type, const std::string& serializedName)
{
	const StorageNode storedNode = m_sqliteStorage.getNodeBySerializedName(serializedName);
	Id id = storedNode.id;

	if (id == 0)
	{
		id = m_sqliteStorage.addNode(type, serializedName);
	}
	else
	{
		if (storedNode.type < type)
		{
			m_sqliteStorage.setNodeType(type, id);
		}
	}

	return id;
}

void PersistentStorage::addFile(const Id id, const std::string& filePath, const std::string& modificationTime)
{
	if (m_sqliteStorage.getFirstById<StorageFile>(id).id == 0)
	{
		m_sqliteStorage.addFile(id, filePath, modificationTime);
	}
}

void PersistentStorage::addSymbol(const Id id, int definitionKind)
{
	if (m_sqliteStorage.getFirstById<StorageSymbol>(id).id == 0)
	{
		m_sqliteStorage.addSymbol(id, definitionKind);
	}
}

Id PersistentStorage::addEdge(int type, Id sourceId, Id targetId)
{
	Id edgeId = m_sqliteStorage.getEdgeBySourceTargetType(sourceId, targetId, type).id;
	if (edgeId == 0)
	{
		edgeId = m_sqliteStorage.addEdge(type, sourceId, targetId);
	}
	return edgeId;
}

Id PersistentStorage::addLocalSymbol(const std::string& name)
{
	Id localSymbolId = m_sqliteStorage.getLocalSymbolByName(name).id;
	if (localSymbolId == 0)
	{
		localSymbolId = m_sqliteStorage.addLocalSymbol(name);
	}
	return localSymbolId;
}

Id PersistentStorage::addSourceLocation(
	Id fileNodeId, uint startLine, uint startCol, uint endLine, uint endCol, int type)
{
	Id sourceLocationId = m_sqliteStorage.getSourceLocationByAll(fileNodeId, startLine, startCol, endLine, endCol, type).id;
	if (sourceLocationId == 0)
	{
		sourceLocationId = m_sqliteStorage.addSourceLocation(
			fileNodeId,
			startLine,
			startCol,
			endLine,
			endCol,
			type
		);
	}
	return sourceLocationId;
}

void PersistentStorage::addOccurrence(Id elementId, Id sourceLocationId)
{
	m_sqliteStorage.addOccurrence(elementId, sourceLocationId);
}

void PersistentStorage::addComponentAccess(Id nodeId , int type)
{
	if (m_sqliteStorage.getComponentAccessByNodeId(nodeId).nodeId == 0)
	{
		m_sqliteStorage.addComponentAccess(nodeId, type);
	}
}

void PersistentStorage::addCommentLocation(Id fileNodeId, uint startLine, uint startCol, uint endLine, uint endCol)
{
	m_sqliteStorage.addCommentLocation(
		fileNodeId,
		startLine,
		startCol,
		endLine,
		endCol
	);
}

void PersistentStorage::addError(
	const std::string& message, const FilePath& filePath, uint startLine, uint startCol, bool fatal, bool indexed)
{
	m_sqliteStorage.addError(
		message,
		filePath,
		startLine,
		startCol,
		fatal,
		indexed
	);
}

void PersistentStorage::forEachNode(std::function<void(const Id /*id*/, const StorageNode& /*data*/)> callback) const
{
	for (StorageNode& node: m_sqliteStorage.getAll<StorageNode>())
	{
		callback(node.id, node);
	}
}

void PersistentStorage::forEachFile(std::function<void(const StorageFile& /*data*/)> callback) const
{
	for (StorageFile& file: m_sqliteStorage.getAll<StorageFile>())
	{
		callback(file);
	}
}

void PersistentStorage::forEachSymbol(std::function<void(const StorageSymbol& /*data*/)> callback) const
{
	for (StorageSymbol& symbol: m_sqliteStorage.getAll<StorageSymbol>())
	{
		callback(symbol);
	}
}

void PersistentStorage::forEachEdge(std::function<void(const Id /*id*/, const StorageEdge& /*data*/)> callback) const
{
	for (StorageEdge& edge: m_sqliteStorage.getAll<StorageEdge>())
	{
		callback(edge.id, edge);
	}
}

void PersistentStorage::forEachLocalSymbol(std::function<void(
	const Id /*id*/, const StorageLocalSymbol& /*data*/)> callback) const
{
	for (StorageLocalSymbol& localSymbol: m_sqliteStorage.getAll<StorageLocalSymbol>())
	{
		callback(localSymbol.id, localSymbol);
	}
}

void PersistentStorage::forEachSourceLocation(std::function<void(const Id /*id*/, const StorageSourceLocation& /*data*/)> callback) const
{
	for (StorageSourceLocation& sourceLocation: m_sqliteStorage.getAll<StorageSourceLocation>())
	{
		callback(sourceLocation.id, sourceLocation);
	}
}

void PersistentStorage::forEachOccurrence(std::function<void(const StorageOccurrence& /*data*/)> callback) const
{
	for (StorageOccurrence& occurrence: m_sqliteStorage.getAll<StorageOccurrence>())
	{
		callback(occurrence);
	}
}

void PersistentStorage::forEachComponentAccess(std::function<void(const StorageComponentAccess& /*data*/)> callback) const
{
	for (StorageComponentAccess& componentAccess: m_sqliteStorage.getAll<StorageComponentAccess>())
	{
		callback(componentAccess);
	}
}

void PersistentStorage::forEachCommentLocation(std::function<void(const StorageCommentLocation& /*data*/)> callback) const
{
	for (StorageCommentLocation& commentLocation: m_sqliteStorage.getAll<StorageCommentLocation>())
	{
		callback(commentLocation);
	}
}

void PersistentStorage::forEachError(std::function<void(const StorageError& /*data*/)> callback) const
{
	for (StorageError& error: m_sqliteStorage.getAll<StorageError>())
	{
		callback(error);
	}
}

void PersistentStorage::startInjection()
{
	m_preInjectionErrorCount = getErrors().size();

	m_sqliteStorage.beginTransaction();
}

void PersistentStorage::finishInjection()
{
	m_sqliteStorage.commitTransaction();

	auto errors = getErrors();

	if (m_preInjectionErrorCount != errors.size())
	{
		MessageNewErrors(std::vector<ErrorInfo>(errors.begin() + m_preInjectionErrorCount, errors.end())).dispatchImmediately();
	}
}

void PersistentStorage::setMode(const SqliteStorage::StorageModeType mode)
{
	m_sqliteStorage.setMode(mode);
}

FilePath PersistentStorage::getDbFilePath() const
{
	return m_sqliteStorage.getDbFilePath();
}

bool PersistentStorage::isEmpty() const
{
	return m_sqliteStorage.isEmpty();
}

bool PersistentStorage::isIncompatible() const
{
	return m_sqliteStorage.isIncompatible();
}

std::string PersistentStorage::getProjectSettingsText() const
{
	return m_sqliteStorage.getProjectSettingsText();
}

void PersistentStorage::setProjectSettingsText(std::string text)
{
	m_sqliteStorage.setProjectSettingsText(text);
}

void PersistentStorage::setup()
{
	m_sqliteStorage.setup();
}

void PersistentStorage::clear()
{
	m_sqliteStorage.clear();

	clearCaches();
}

void PersistentStorage::clearCaches()
{
	m_symbolIndex.clear();
	m_fileIndex.clear();
	m_fileNodeIds.clear();
	m_fileNodePaths.clear();
	m_hierarchyCache.clear();
	m_fullTextSearchIndex.clear();
}

std::set<FilePath> PersistentStorage::getReferenced(const std::set<FilePath>& filePaths)
{
	TRACE();
	std::set<FilePath> referenced;

	utility::append(referenced, getReferencedByIncludes(filePaths));
	utility::append(referenced, getReferencedByImports(filePaths));

	return referenced;
}

std::set<FilePath> PersistentStorage::getReferencing(const std::set<FilePath>& filePaths)
{
	TRACE();
	std::set<FilePath> referencing;

	utility::append(referencing, getReferencingByIncludes(filePaths));
	utility::append(referencing, getReferencingByImports(filePaths));

	return referencing;
}

void PersistentStorage::clearFileElements(const std::vector<FilePath>& filePaths, std::function<void(int)> updateStatusCallback)
{
	TRACE();

	const std::vector<Id> fileNodeIds = getFileNodeIds(filePaths);

	if (!fileNodeIds.empty())
	{
		m_sqliteStorage.removeElementsWithLocationInFiles(fileNodeIds, updateStatusCallback);
		m_sqliteStorage.removeElements(fileNodeIds);

		m_sqliteStorage.removeErrorsInFiles(filePaths);
	}
}

std::vector<FileInfo> PersistentStorage::getInfoOnAllFiles() const
{
	TRACE();

	std::vector<FileInfo> fileInfos;

	std::vector<StorageFile> storageFiles = m_sqliteStorage.getAll<StorageFile>();
	for (size_t i = 0; i < storageFiles.size(); i++)
	{
		boost::posix_time::ptime modificationTime = boost::posix_time::not_a_date_time;
		if (storageFiles[i].modificationTime != "not-a-date-time")
		{
			modificationTime = boost::posix_time::time_from_string(storageFiles[i].modificationTime);
		}
		fileInfos.push_back(FileInfo(
			FilePath(storageFiles[i].filePath),
			modificationTime
		));
	}

	return fileInfos;
}

void PersistentStorage::buildCaches()
{
	TRACE();

	clearCaches();

	buildFilePathMaps();
	buildSearchIndex();
	buildHierarchyCache();
}

void PersistentStorage::optimizeMemory()
{
	TRACE();

	m_sqliteStorage.optimizeMemory();
	m_sqliteStorage.setVersion();
}

Id PersistentStorage::getIdForNodeWithNameHierarchy(const NameHierarchy& nameHierarchy) const
{
	return m_sqliteStorage.getNodeBySerializedName(NameHierarchy::serialize(nameHierarchy)).id;
}

Id PersistentStorage::getIdForEdge(
	Edge::EdgeType type, const NameHierarchy& fromNameHierarchy, const NameHierarchy& toNameHierarchy
) const
{
	Id sourceId = getIdForNodeWithNameHierarchy(fromNameHierarchy);
	Id targetId = getIdForNodeWithNameHierarchy(toNameHierarchy);
	return m_sqliteStorage.getEdgeBySourceTargetType(sourceId, targetId, type).id;
}

NameHierarchy PersistentStorage::getNameHierarchyForNodeWithId(Id nodeId) const
{
	TRACE();

	return NameHierarchy::deserialize(m_sqliteStorage.getFirstById<StorageNode>(nodeId).serializedName);
}

Node::NodeType PersistentStorage::getNodeTypeForNodeWithId(Id nodeId) const
{
	return Node::intToType(m_sqliteStorage.getFirstById<StorageNode>(nodeId).type);
}

std::shared_ptr<TokenLocationCollection> PersistentStorage::getFullTextSearchLocations(
		const std::string& searchTerm, bool caseSensitive
) const
{
	TRACE();

	std::shared_ptr<TokenLocationCollection> collection = std::make_shared<TokenLocationCollection>();
	if (!searchTerm.size())
	{
		return collection;
	}

	if (m_fullTextSearchIndex.fileCount() == 0)
	{
		MessageStatus("Building fulltext search index", false, true).dispatch();
		buildFullTextSearchIndex();
	}

	MessageStatus(
		std::string("Searching fulltext (case-") + (caseSensitive ? "sensitive" : "insensitive") + "): " + searchTerm,
		false, true
	).dispatch();

	std::vector<FullTextSearchResult> hits = m_fullTextSearchIndex.searchForTerm(searchTerm);

	int termLength = searchTerm.length();
	FilePath filepath;
	std::shared_ptr<TextAccess> file;
	ParseLocation location;
	for (size_t i = 0; i < hits.size(); i++)
	{
		filepath = getFileNodePath(hits[i].fileId);
		file = getFileContent(filepath);

		int charsInPreviousLines = 0;
		int lineNumber = 1;
		std::string line;
		line = file->getLine(lineNumber);

		for (int pos : hits[i].positions)
		{
			bool addHit = true;
			while( (charsInPreviousLines + (int)line.length()) < pos)
			{
				lineNumber++;
				charsInPreviousLines += line.length();
				line = file->getLine(lineNumber);
			}
			location.startLineNumber = lineNumber;
			location.startColumnNumber = pos - charsInPreviousLines + 1;

			if ( caseSensitive )
			{
				if( line.substr(location.startColumnNumber-1, termLength) != searchTerm )
				{
					addHit = false;
				}
			}

			while( (charsInPreviousLines + (int)line.length()) < pos + termLength)
			{
				lineNumber++;
				charsInPreviousLines += line.length();
				line = file->getLine(lineNumber);
			}

			location.endLineNumber = lineNumber;
			location.endColumnNumber = pos + termLength - charsInPreviousLines;

			if ( addHit )
			{
				// Set first bit to 1 to avoid collisions
				Id locationId = ~(~size_t(0) >> 1) + collection->getTokenLocationCount();

				collection->addTokenLocation(
					locationId,
					0,
					filepath,
					location.startLineNumber,
					location.startColumnNumber,
					location.endLineNumber,
					location.endColumnNumber
				)->setType(LOCATION_FULLTEXT);
			}
		}
	}

	MessageStatus(
		std::to_string(collection->getTokenLocationCount()) + " results in " +
			std::to_string(collection->getTokenLocationFileCount()) + " files for fulltext search (case-" +
			(caseSensitive ? "sensitive" : "insensitive") + "): " + searchTerm,
		false, false
	).dispatch();

	return collection;
}

std::vector<SearchMatch> PersistentStorage::getAutocompletionMatches(const std::string& query) const
{
	TRACE();

	// search in indices
	size_t maxResultsCount = 100;
	size_t maxBestScoredResultsLength = 100;

	// create SearchMatches
	std::set<SearchMatch> matchesSet;
	utility::append(matchesSet, getAutocompletionSymbolMatches(query, maxResultsCount));
	utility::append(matchesSet, getAutocompletionFileMatches(query, 20));
	utility::append(matchesSet, getAutocompletionCommandMatches(query));

	std::vector<SearchMatch> matches = utility::toVector(matchesSet);

	for (auto it = matches.begin(); it != matches.end(); it++)
	{
		SearchMatch& match = *it;
		// rescore match
		if (!match.subtext.empty() && match.indices.size())
		{
			SearchResult newResult =
				SearchIndex::rescoreText(match.name, match.text, match.indices, match.score, maxBestScoredResultsLength);

			match.score = newResult.score;
			match.indices = newResult.indices;
		}
	}

	if (matches.size() > maxResultsCount)
	{
		matches.resize(maxResultsCount);
	}

	return matches;
}

std::set<SearchMatch> PersistentStorage::getAutocompletionSymbolMatches(const std::string& query, size_t maxResultsCount) const
{
	// search in indices
	std::vector<SearchResult> results = m_symbolIndex.search(query, maxResultsCount, maxResultsCount);

	// fetch StorageNodes for node ids
	std::map<Id, StorageNode> storageNodeMap;
	std::map<Id, StorageSymbol> storageSymbolMap;
	{
		std::vector<Id> elementIds;

		for (const SearchResult& result : results)
		{
			elementIds.insert(elementIds.end(), result.elementIds.begin(), result.elementIds.end());
		}

		for (StorageNode& node : m_sqliteStorage.getAllByIds<StorageNode>(elementIds))
		{
			storageNodeMap[node.id] = node;
		}

		for (StorageSymbol& symbol : m_sqliteStorage.getAllByIds<StorageSymbol>(elementIds))
		{
			storageSymbolMap[symbol.id] = symbol;
		}
	}

	// create SearchMatches
	std::set<SearchMatch> matches;
	for (const SearchResult& result : results)
	{
		SearchMatch match;

		const StorageNode* firstNode = nullptr;
		for (const Id& elementId : result.elementIds)
		{
			if (elementId != 0)
			{
				const StorageNode& node = storageNodeMap[elementId];
				match.nameHierarchies.push_back(NameHierarchy::deserialize(node.serializedName));

				if (!match.hasChildren)
				{
					match.hasChildren = m_hierarchyCache.nodeHasChildren(node.id);
				}

				if (!firstNode)
				{
					firstNode = &node;
				}
			}
		}

		match.name = result.text;

		match.text = result.text;
		const size_t idx = m_hierarchyCache.getIndexOfLastVisibleParentNode(firstNode->id);
		const NameHierarchy& name = match.nameHierarchies[0];
		match.text = name.getRange(idx, name.size()).getQualifiedName();
		match.subtext = name.getRange(0, idx).getQualifiedName();

		match.indices = result.indices;
		match.score = result.score;
		match.nodeType = Node::intToType(firstNode->type);
		match.typeName = Node::getReadableTypeString(match.nodeType);
		match.searchType = SearchMatch::SEARCH_TOKEN;

		if (storageSymbolMap.find(firstNode->id) == storageSymbolMap.end() &&
			match.nodeType != Node::NODE_NON_INDEXED)
		{
			match.typeName = "non-indexed " + match.typeName;
		}

		matches.insert(match);
	}

	return matches;
}

std::set<SearchMatch> PersistentStorage::getAutocompletionFileMatches(const std::string& query, size_t maxResultsCount) const
{
	std::vector<SearchResult> results = m_fileIndex.search(query, maxResultsCount);

	// create SearchMatches
	std::set<SearchMatch> matches;
	for (const SearchResult& result : results)
	{
		SearchMatch match;

		match.nameHierarchies.push_back(NameHierarchy(result.text));

		match.name = result.text;

		FilePath path(match.name);
		match.text = path.fileName();
		match.subtext = path.str();

		match.indices = result.indices;
		match.score = result.score;

		match.nodeType = Node::NODE_FILE;
		match.typeName = Node::getReadableTypeString(match.nodeType);

		match.searchType = SearchMatch::SEARCH_TOKEN;

		matches.insert(match);
	}

	return matches;
}

std::set<SearchMatch> PersistentStorage::getAutocompletionCommandMatches(const std::string& query) const
{
	// search in indices
	std::vector<SearchResult> results = m_commandIndex.search(query, 0);

	// create SearchMatches
	std::set<SearchMatch> matches;
	for (const SearchResult& result : results)
	{
		SearchMatch match;

		match.name = result.text;
		match.text = result.text;
		match.indices = result.indices;
		match.score = result.score;

		match.searchType = SearchMatch::SEARCH_COMMAND;
		match.typeName = "command";

		matches.insert(match);
	}

	return matches;
}

std::vector<SearchMatch> PersistentStorage::getSearchMatchesForTokenIds(const std::vector<Id>& elementIds) const
{
	// todo: what if all these elements share the same node in the searchindex?
	// In that case there should be only one search match.
	std::vector<SearchMatch> matches;

	for (Id elementId : elementIds)
	{
		SearchMatch match;

		NameHierarchy nameHierarchy = NameHierarchy::deserialize(m_sqliteStorage.getFirstById<StorageNode>(elementId).serializedName);
		match.name = nameHierarchy.getQualifiedName();
		match.text = nameHierarchy.getRawName();
		match.nameHierarchies.push_back(nameHierarchy.getQualifiedName());
		match.searchType = SearchMatch::SEARCH_TOKEN;

		if (m_sqliteStorage.isFile(elementId))
		{
			match.text = FilePath(match.text).fileName();
			match.nodeType = Node::NODE_FILE;
		}
		else if (m_sqliteStorage.isNode(elementId))
		{
			StorageNode node = m_sqliteStorage.getFirstById<StorageNode>(elementId);
			match.nodeType = Node::intToType(node.type);
		}
		else
		{
			continue;
		}

		matches.push_back(match);
	}

	return matches;
}

std::shared_ptr<Graph> PersistentStorage::getGraphForAll() const
{
	TRACE();

	std::shared_ptr<Graph> graph = std::make_shared<Graph>();

	std::unordered_set<Id> explicitlyDefinedSymbolIds;
	for (StorageSymbol symbol: m_sqliteStorage.getAll<StorageSymbol>())
	{
		if (intToDefinitionKind(symbol.definitionKind) == DEFINITION_EXPLICIT)
		{
			explicitlyDefinedSymbolIds.insert(symbol.id);
		}
	}

	std::vector<Id> tokenIds;
	for (StorageNode node: m_sqliteStorage.getAll<StorageNode>())
	{
		if (explicitlyDefinedSymbolIds.find(node.id) != explicitlyDefinedSymbolIds.end() &&
			(
				!m_hierarchyCache.isChildOfVisibleNodeOrInvisible(node.id) ||
				(
					Node::intToType(node.type) == Node::NODE_NAMESPACE || // TODO: use & operator here
					Node::intToType(node.type) == Node::NODE_PACKAGE
					)
				)
			)
		{
			tokenIds.push_back(node.id);
		}
	}

	for (StorageFile file: m_sqliteStorage.getAll<StorageFile>())
	{
		tokenIds.push_back(file.id);
	}

	addNodesToGraph(tokenIds, graph.get());

	return graph;
}

std::shared_ptr<Graph> PersistentStorage::getGraphForActiveTokenIds(const std::vector<Id>& tokenIds, bool* isActiveNamespace) const
{
	TRACE();

	std::shared_ptr<Graph> g = std::make_shared<Graph>();
	Graph* graph = g.get();

	std::vector<Id> ids(tokenIds);
	bool isNamespace = false;

	std::vector<Id> nodeIds;
	std::vector<Id> edgeIds;

	bool addAggregations = false;
	std::vector<StorageEdge> edgesToAggregate;

	if (tokenIds.size() == 1)
	{
		const Id elementId = tokenIds[0];
		StorageNode node = m_sqliteStorage.getFirstById<StorageNode>(elementId);

		if (node.id > 0)
		{
			Node::NodeType nodeType = Node::intToType(node.type);
			if (nodeType & (Node::NODE_NAMESPACE | Node::NODE_PACKAGE))
			{
				ids.clear();
				m_hierarchyCache.addFirstChildIdsForNodeId(elementId, &ids);

				isNamespace = true;
			}
			else
			{
				nodeIds.push_back(elementId);

				std::vector<StorageEdge> edges = m_sqliteStorage.getEdgesBySourceOrTargetId(elementId);
				for (const StorageEdge& edge : edges)
				{
					Edge::EdgeType edgeType = Edge::intToType(edge.type);
					if (edgeType == Edge::EDGE_MEMBER)
					{
						continue;
					}

					if ((nodeType & Node::NODE_USEABLE_TYPE) && (edgeType & Edge::EDGE_TYPE_USAGE) &&
						m_hierarchyCache.isChildOfVisibleNodeOrInvisible(edge.sourceNodeId))
					{
						edgesToAggregate.push_back(edge);
					}
					else
					{
						edgeIds.push_back(edge.id);
					}
				}

				addAggregations = true;
			}
		}
		else if (m_sqliteStorage.isEdge(elementId))
		{
			edgeIds.push_back(elementId);
		}
	}

	if (ids.size() >= 1 || isNamespace)
	{
		for (const StorageSymbol& symbol : m_sqliteStorage.getAllByIds<StorageSymbol>(ids))
		{
			if (symbol.id > 0 && (!isNamespace || intToDefinitionKind(symbol.definitionKind) != DEFINITION_IMPLICIT))
			{
				nodeIds.push_back(symbol.id);
			}
		}
		for (const StorageFile& file : m_sqliteStorage.getAllByIds<StorageFile>(ids))
		{
			nodeIds.push_back(file.id);
		}

		if (nodeIds.size() != ids.size())
		{
			std::vector<StorageEdge> edges = m_sqliteStorage.getAllByIds<StorageEdge>(ids);
			for (const StorageEdge& edge : edges)
			{
				if (edge.id > 0)
				{
					edgeIds.push_back(edge.id);
				}
			}
		}
	}

	if (isNamespace)
	{
		addNodesToGraph(nodeIds, graph);
	}
	else
	{
		addNodesWithChildrenAndEdgesToGraph(nodeIds, edgeIds, graph);
	}

	if (addAggregations)
	{
		addAggregationEdgesToGraph(tokenIds[0], edgesToAggregate, graph);
	}

	addComponentAccessToGraph(graph);

	if (isActiveNamespace)
	{
		*isActiveNamespace = isNamespace;
	}

	return g;
}

// TODO: rename: getActiveElementIdsForId; TODO: make separate function for declarationId
std::vector<Id> PersistentStorage::getActiveTokenIdsForId(Id tokenId, Id* declarationId) const
{
	std::vector<Id> activeTokenIds;

	if (!(m_sqliteStorage.isEdge(tokenId) || m_sqliteStorage.isNode(tokenId)))
	{
		return activeTokenIds;
	}

	activeTokenIds.push_back(tokenId);

	if (m_sqliteStorage.isNode(tokenId))
	{
		*declarationId = tokenId;

		std::vector<StorageEdge> incomingEdges = m_sqliteStorage.getEdgesByTargetId(tokenId);
		for (size_t i = 0; i < incomingEdges.size(); i++)
		{
			activeTokenIds.push_back(incomingEdges[i].id);
		}
	}

	return activeTokenIds;
}

std::vector<Id> PersistentStorage::getNodeIdsForLocationIds(const std::vector<Id>& locationIds) const
{
	TRACE();

	std::set<Id> edgeIds;
	std::set<Id> nodeIds;
	std::set<Id> implicitNodeIds;

	for (const StorageOccurrence& occurrence: m_sqliteStorage.getOccurrencesForLocationIds(locationIds))
	{
		const Id elementId = occurrence.elementId;

		StorageEdge edge = m_sqliteStorage.getFirstById<StorageEdge>(elementId);
		if (edge.id != 0) // here we test if location is an edge.
		{
			edgeIds.insert(edge.targetNodeId);
		}
		else if(m_sqliteStorage.isNode(elementId))
		{
			StorageSymbol symbol = m_sqliteStorage.getFirstById<StorageSymbol>(elementId);
			if (symbol.id != 0) // here we test if location is a symbol
			{
				if (intToDefinitionKind(symbol.definitionKind) == DEFINITION_IMPLICIT)
				{
					implicitNodeIds.insert(elementId);
				}
				else
				{
					nodeIds.insert(elementId);
				}
			}
			else // is file
			{
				nodeIds.insert(elementId);
			}
		}
	}

	if (nodeIds.size() == 0)
	{
		nodeIds = implicitNodeIds;
	}

	if (nodeIds.size())
	{
		return utility::toVector(nodeIds);
	}

	return utility::toVector(edgeIds);
}

std::vector<Id> PersistentStorage::getLocalSymbolIdsForLocationIds(const std::vector<Id>& locationIds) const
{
	std::set<Id> localSymbolIds;

	for (const StorageOccurrence& occurrence: m_sqliteStorage.getOccurrencesForLocationIds(locationIds))
	{
		Id elementId = occurrence.elementId;

		if (m_sqliteStorage.getFirstById<StorageNode>(elementId).id == 0 && m_sqliteStorage.getFirstById<StorageEdge>(elementId).id == 0)
		{
			localSymbolIds.insert(elementId);
		}
	}

	return utility::toVector(localSymbolIds);
}

std::vector<Id> PersistentStorage::getTokenIdsForMatches(const std::vector<SearchMatch>& matches) const
{
	FilePath rootPath = getDbFilePath().parentDirectory();
	std::set<Id> idSet;
	for (const SearchMatch& match : matches)
	{
		if (match.nodeType == Node::NODE_FILE)
		{
			idSet.insert(m_sqliteStorage.getFileByPath(rootPath.concat(match.subtext).str()).id);
		}
		else
		{
			for (size_t i = 0; i < match.nameHierarchies.size(); i++)
			{
				idSet.insert(
					m_sqliteStorage.getNodeBySerializedName(NameHierarchy::serialize(match.nameHierarchies[i])).id
				);
			}
		}
	}

	std::vector<Id> ids;
	for (std::set<Id>::const_iterator it = idSet.begin(); it != idSet.end(); it++)
	{
		if (*it != 0)
		{
			ids.push_back(*it);
		}
	}

	return ids;
}

Id PersistentStorage::getTokenIdForFileNode(const FilePath& filePath) const
{
	return m_sqliteStorage.getFileByPath(filePath.str()).id;
}

std::shared_ptr<TokenLocationCollection> PersistentStorage::getTokenLocationsForTokenIds(
	const std::vector<Id>& tokenIds) const
{
	TRACE();

	std::shared_ptr<TokenLocationCollection> collection = std::make_shared<TokenLocationCollection>();

	std::vector<Id> fileIds;
	std::vector<Id> nonFileIds;

	for (const Id tokenId : tokenIds)
	{
		if (!getFileNodePath(tokenId).empty())
		{
			fileIds.push_back(tokenId);
		}
		else
		{
			nonFileIds.push_back(tokenId);
		}
	}

	for (StorageFile file: m_sqliteStorage.getAllByIds<StorageFile>(fileIds))
	{
		collection->addTokenLocationFile(m_sqliteStorage.getTokenLocationsForFile(file.filePath));
	}

	{
		std::vector<Id> locationIds;
		std::unordered_map<Id, Id> locationIdToElementIdMap;
		for (const StorageOccurrence& occurrence: m_sqliteStorage.getOccurrencesForElementIds(nonFileIds))
		{
			locationIds.push_back(occurrence.sourceLocationId);
			locationIdToElementIdMap[occurrence.sourceLocationId] = occurrence.elementId;
		}

		for (const StorageSourceLocation& sourceLocation: m_sqliteStorage.getAllByIds<StorageSourceLocation>(locationIds))
		{
			auto it = locationIdToElementIdMap.find(sourceLocation.id);
			if (it != locationIdToElementIdMap.end())
			{
				TokenLocation* tokenLocation = collection->addTokenLocation(
					sourceLocation.id,
					it->second,
					getFileNodePath(sourceLocation.fileNodeId),
					sourceLocation.startLine,
					sourceLocation.startCol,
					sourceLocation.endLine,
					sourceLocation.endCol
				);

				if (tokenLocation)
				{
					tokenLocation->setType(intToLocationType(sourceLocation.type));
				}
			}
		}
	}
	return collection;
}

std::shared_ptr<TokenLocationCollection> PersistentStorage::getTokenLocationsForLocationIds(
		const std::vector<Id>& locationIds
) const
{
	TRACE();

	std::shared_ptr<TokenLocationCollection> collection = std::make_shared<TokenLocationCollection>();

	for (StorageSourceLocation location: m_sqliteStorage.getAllByIds<StorageSourceLocation>(locationIds))
	{
		for (const StorageOccurrence& occurrences: m_sqliteStorage.getOccurrencesForLocationId(location.id))
		{
			collection->addTokenLocation(
				location.id,
				occurrences.elementId,
				getFileNodePath(location.fileNodeId),
				location.startLine,
				location.startCol,
				location.endLine,
				location.endCol
			)->setType(intToLocationType(location.type));
		}
	}

	return collection;
}

std::shared_ptr<TokenLocationFile> PersistentStorage::getTokenLocationsForFile(const std::string& filePath) const
{
	TRACE();

	return m_sqliteStorage.getTokenLocationsForFile(filePath);
}

std::shared_ptr<TokenLocationFile> PersistentStorage::getTokenLocationsForLinesInFile(
		const std::string& filePath, uint firstLineNumber, uint lastLineNumber
) const
{
	TRACE();

	return getTokenLocationsForFile(filePath)->getFilteredByLines(firstLineNumber, lastLineNumber);
}

std::shared_ptr<TokenLocationFile> PersistentStorage::getCommentLocationsInFile(const FilePath& filePath) const
{
	TRACE();

	std::shared_ptr<TokenLocationFile> file = std::make_shared<TokenLocationFile>(filePath);

	std::vector<StorageCommentLocation> storageLocations = m_sqliteStorage.getCommentLocationsInFile(filePath);
	for (size_t i = 0; i < storageLocations.size(); i++)
	{
		file->addTokenLocation(
			storageLocations[i].id,
			0, // comment token location has no element.
			storageLocations[i].startLine,
			storageLocations[i].startCol,
			storageLocations[i].endLine,
			storageLocations[i].endCol
		);
	}

	return file;
}

std::shared_ptr<TextAccess> PersistentStorage::getFileContent(const FilePath& filePath) const
{
	return m_sqliteStorage.getFileContentByPath(filePath.str());
}

FileInfo PersistentStorage::getFileInfoForFilePath(const FilePath& filePath) const
{
	return FileInfo(filePath, m_sqliteStorage.getFileByPath(filePath.str()).modificationTime);
}

std::vector<FileInfo> PersistentStorage::getFileInfosForFilePaths(const std::vector<FilePath>& filePaths) const
{
	std::vector<FileInfo> fileInfos;

	std::vector<StorageFile> storageFiles = m_sqliteStorage.getFilesByPaths(filePaths);
	for (const StorageFile& file : storageFiles)
	{
		fileInfos.push_back(FileInfo(FilePath(file.filePath), file.modificationTime));
	}

	return fileInfos;
}

StorageStats PersistentStorage::getStorageStats() const
{
	TRACE();

	StorageStats stats;

	stats.nodeCount = m_sqliteStorage.getNodeCount();
	stats.edgeCount = m_sqliteStorage.getEdgeCount();

	stats.fileCount = m_sqliteStorage.getFileCount();
	stats.fileLOCCount = m_sqliteStorage.getFileLineSum();

	return stats;
}

ErrorCountInfo PersistentStorage::getErrorCount() const
{
	ErrorCountInfo info;

	std::vector<ErrorInfo> errors = getErrors();
	for (const ErrorInfo& error : errors)
	{
		info.total++;

		if (error.fatal)
		{
			info.fatal++;
		}
	}

	return info;
}

std::vector<ErrorInfo> PersistentStorage::getErrors() const
{
	std::vector<ErrorInfo> errors = m_sqliteStorage.getAll<StorageError>();
	std::vector<ErrorInfo> filteredErrors;

	for (const ErrorInfo& error : errors)
	{
		if (m_errorFilter.filter(error))
		{
			filteredErrors.push_back(error);
		}
	}

	return filteredErrors;
}

std::shared_ptr<TokenLocationCollection> PersistentStorage::getErrorTokenLocations(std::vector<ErrorInfo>* errors) const
{
	TRACE();

	std::shared_ptr<TokenLocationCollection> errorCollection = std::make_shared<TokenLocationCollection>();
	for (const ErrorInfo& error : m_sqliteStorage.getAll<StorageError>())
	{
		if (m_errorFilter.filter(error))
		{
			errors->push_back(error);

			// Set first bit to 1 to avoid collisions
			Id locationId = ~(~size_t(0) >> 1) + error.id;

			errorCollection->addTokenLocation(
				locationId, error.id, error.filePath, error.lineNumber, error.columnNumber, error.lineNumber, error.columnNumber
			)->setType(LOCATION_ERROR);
		}
	}

	return errorCollection;
}

Id PersistentStorage::getFileNodeId(const FilePath& filePath) const
{
	if (filePath.empty())
	{
		LOG_ERROR("No file path set");
		return 0;
	}

	std::map<FilePath, Id>::const_iterator it = m_fileNodeIds.find(filePath);

	if (it != m_fileNodeIds.end())
	{
		return it->second;
	}

	return 0;
}

std::vector<Id> PersistentStorage::getFileNodeIds(const std::vector<FilePath>& filePaths) const
{
	std::vector<Id> ids;
	for (const FilePath& path : filePaths)
	{
		ids.push_back(getFileNodeId(path));
	}
	return ids;
}

std::set<Id> PersistentStorage::getFileNodeIds(const std::set<FilePath>& filePaths) const
{
	std::set<Id> ids;
	for (const FilePath& path : filePaths)
	{
		ids.insert(getFileNodeId(path));
	}
	return ids;
}

FilePath PersistentStorage::getFileNodePath(Id fileId) const
{
	if (fileId == 0)
	{
		LOG_ERROR("No file id set");
		return FilePath();
	}

	std::map<Id, FilePath>::const_iterator it = m_fileNodePaths.find(fileId);

	if (it != m_fileNodePaths.end())
	{
		return it->second;
	}

	return FilePath();
}

std::unordered_map<Id, std::set<Id>> PersistentStorage::getFileIdToIncludingFileIdMap() const
{
	std::unordered_map<Id, std::set<Id>> fileIdToIncludingFileIdMap;
	for (const StorageEdge& includeEdge : m_sqliteStorage.getEdgesByType(Edge::typeToInt(Edge::EDGE_INCLUDE)))
	{
		fileIdToIncludingFileIdMap[includeEdge.targetNodeId].insert(includeEdge.sourceNodeId);
	}
	return fileIdToIncludingFileIdMap;
}

std::unordered_map<Id, std::set<Id>> PersistentStorage::getFileIdToImportingFileIdMap() const
{
	std::unordered_map<Id, std::set<Id>> fileIdToImportingFileIdMap;
	{
		std::vector<Id> importedElementIds;
		std::map<Id, std::set<Id>> elementIdToImportingFileIds;

		for (const StorageEdge& importEdge : m_sqliteStorage.getEdgesByType(Edge::typeToInt(Edge::EDGE_IMPORT)))
		{
			importedElementIds.push_back(importEdge.targetNodeId);
			elementIdToImportingFileIds[importEdge.targetNodeId].insert(importEdge.sourceNodeId);
		}

		std::unordered_map<Id, Id> importedElementIdToFileNodeId;
		{
			std::vector<Id> importedSourceLocationIds;
			std::unordered_map<Id, Id> importedSourceLocationToElementIds;
			for (const StorageOccurrence& occurrence: m_sqliteStorage.getOccurrencesForElementIds(importedElementIds))
			{
				importedSourceLocationIds.push_back(occurrence.sourceLocationId);
				importedSourceLocationToElementIds[occurrence.sourceLocationId] = occurrence.elementId;
			}

			for (const StorageSourceLocation& sourceLocation: m_sqliteStorage.getAllByIds<StorageSourceLocation>(importedSourceLocationIds))
			{
				auto it = importedSourceLocationToElementIds.find(sourceLocation.id);
				if (it != importedSourceLocationToElementIds.end())
				{
					importedElementIdToFileNodeId[it->second] = sourceLocation.fileNodeId;
				}
			}
		}

		for (const auto& it: elementIdToImportingFileIds)
		{
			auto importedFileIt = importedElementIdToFileNodeId.find(it.first);
			if (importedFileIt != importedElementIdToFileNodeId.end())
			{
				fileIdToImportingFileIdMap[importedFileIt->second].insert(it.second.begin(), it.second.end());
			}
		}
	}
	return fileIdToImportingFileIdMap;
}

std::set<Id> PersistentStorage::getReferenced(const std::set<Id>& ids, std::unordered_map<Id, std::set<Id>> idToReferencingIdMap) const
{
	std::unordered_map<Id, std::set<Id>> idToReferencedIdMap;
	for (auto it: idToReferencingIdMap)
	{
		for (Id referencingId: it.second)
		{
			idToReferencedIdMap[referencingId].insert(it.first);
		}
	}

	return getReferencing(ids, idToReferencedIdMap);
}

std::set<Id> PersistentStorage::getReferencing(const std::set<Id>& ids, std::unordered_map<Id, std::set<Id>> idToReferencingIdMap) const
{
	std::set<Id> referencingIds;

	std::set<Id> processingIds = ids;
	std::set<Id> processedIds;

	while (!processingIds.empty())
	{
		std::set<Id> tempIds = processingIds;
		utility::append(processedIds, processingIds);
		processingIds.clear();

		for (Id id: tempIds)
		{
			utility::append(referencingIds, idToReferencingIdMap[id]);
			for (Id referencingId: idToReferencingIdMap[id])
			{
				if (processedIds.find(referencingId) == processedIds.end())
				{
					processingIds.insert(referencingId);
				}
			}
		}
	}

	return referencingIds;

}

std::set<FilePath> PersistentStorage::getReferencedByIncludes(const std::set<FilePath>& filePaths)
{
	std::set<Id> ids = getReferenced(getFileNodeIds(filePaths), getFileIdToIncludingFileIdMap());

	std::set<FilePath> paths;
	for (Id id: ids)
	{
		paths.insert(getFileNodePath(id));
	}

	return paths;
}

std::set<FilePath> PersistentStorage::getReferencedByImports(const std::set<FilePath>& filePaths)
{
	std::set<Id> ids = getReferenced(getFileNodeIds(filePaths), getFileIdToImportingFileIdMap());

	std::set<FilePath> paths;
	for (Id id: ids)
	{
		paths.insert(getFileNodePath(id));
	}

	return paths;
}

std::set<FilePath> PersistentStorage::getReferencingByIncludes(const std::set<FilePath>& filePaths)
{
	std::set<Id> ids = getReferencing(getFileNodeIds(filePaths), getFileIdToIncludingFileIdMap());

	std::set<FilePath> paths;
	for (Id id: ids)
	{
		paths.insert(getFileNodePath(id));
	}

	return paths;
}

std::set<FilePath> PersistentStorage::getReferencingByImports(const std::set<FilePath>& filePaths)
{
	std::set<Id> ids = getReferencing(getFileNodeIds(filePaths), getFileIdToImportingFileIdMap());

	std::set<FilePath> paths;
	for (Id id: ids)
	{
		paths.insert(getFileNodePath(id));
	}

	return paths;
}

Id PersistentStorage::getLastVisibleParentNodeId(const Id nodeId) const
{
	return m_hierarchyCache.getLastVisibleParentNodeId(nodeId);
}

std::vector<Id> PersistentStorage::getAllChildNodeIds(const Id nodeId) const
{
	std::vector<Id> childNodeIds;
	std::vector<Id> edgeIds;

	m_hierarchyCache.addAllChildIdsForNodeId(nodeId, &childNodeIds, &edgeIds);

	return childNodeIds;
}

void PersistentStorage::addNodesToGraph(const std::vector<Id>& nodeIds, Graph* graph) const
{
	TRACE();

	if (nodeIds.size() == 0)
	{
		return;
	}

	std::unordered_map<Id, StorageSymbol> symbolMap;
	for (const StorageSymbol& symbol : m_sqliteStorage.getAllByIds<StorageSymbol>(nodeIds))
	{
		symbolMap[symbol.id] = symbol;
	}

	for (const StorageNode& storageNode : m_sqliteStorage.getAllByIds<StorageNode>(nodeIds))
	{
		const Node::NodeType type = Node::intToType(storageNode.type);
		if (type == Node::NODE_FILE)
		{
			const FilePath filePath(NameHierarchy::deserialize(storageNode.serializedName).getRawName());
			Node* node = graph->createNode(
				storageNode.id,
				Node::NODE_FILE,
				NameHierarchy(filePath.fileName()),
				true
			);
			node->addComponentFilePath(std::make_shared<TokenComponentFilePath>(filePath));
			node->setExplicit(true);
		}
		else
		{
			const NameHierarchy nameHierarchy = NameHierarchy::deserialize(storageNode.serializedName);

			DefinitionKind defKind = DEFINITION_NONE;
			auto it = symbolMap.find(storageNode.id);
			if (it != symbolMap.end())
			{
				defKind = intToDefinitionKind(it->second.definitionKind);
			}

			Node* node = graph->createNode(
				storageNode.id,
				type,
				nameHierarchy,
				defKind != DEFINITION_NONE
			);

			if (defKind == DEFINITION_IMPLICIT)
			{
				node->setImplicit(true);
			}
			else if (defKind == DEFINITION_EXPLICIT)
			{
				node->setExplicit(true);
			}

			if (type == Node::NODE_FUNCTION || type == Node::NODE_METHOD)
			{
				std::string signatureString = nameHierarchy.getRawNameWithSignature();
				if (signatureString.size() > 0) // this should always be the case since functions and methods must have sigs.
				{
					node->addComponentSignature(
						std::make_shared<TokenComponentSignature>(signatureString)
					);
				}
			}
		}
	}
}

void PersistentStorage::addEdgesToGraph(const std::vector<Id>& edgeIds, Graph* graph) const
{
	TRACE();

	if (edgeIds.size() == 0)
	{
		return;
	}

	for (const StorageEdge& storageEdge : m_sqliteStorage.getAllByIds<StorageEdge>(edgeIds))
	{
		Node* sourceNode = graph->getNodeById(storageEdge.sourceNodeId);
		Node* targetNode = graph->getNodeById(storageEdge.targetNodeId);

		if (sourceNode && targetNode)
		{
			graph->createEdge(storageEdge.id, Edge::intToType(storageEdge.type), sourceNode, targetNode);
		}
		else
		{
			LOG_ERROR("Can't add edge because nodes are not present");
		}
	}
}

void PersistentStorage::addNodesWithChildrenAndEdgesToGraph(
	const std::vector<Id>& nodeIds, const std::vector<Id>& edgeIds, Graph* graph
) const
{
	TRACE();

	std::set<Id> parentNodeIds;

	for (Id nodeId : nodeIds)
	{
		parentNodeIds.insert(getLastVisibleParentNodeId(nodeId));
	}

	if (edgeIds.size() > 0)
	{
		for (const StorageEdge& storageEdge : m_sqliteStorage.getAllByIds<StorageEdge>(edgeIds))
		{
			parentNodeIds.insert(getLastVisibleParentNodeId(storageEdge.sourceNodeId));
			parentNodeIds.insert(getLastVisibleParentNodeId(storageEdge.targetNodeId));
		}
	}

	std::vector<Id> allNodeIds;
	std::vector<Id> allEdgeIds = edgeIds;

	for (Id parentNodeId : parentNodeIds)
	{
		allNodeIds.push_back(parentNodeId);
		m_hierarchyCache.addAllChildIdsForNodeId(parentNodeId, &allNodeIds, &allEdgeIds);
	}

	addNodesToGraph(allNodeIds, graph);
	addEdgesToGraph(allEdgeIds, graph);
}

void PersistentStorage::addAggregationEdgesToGraph(
	const Id nodeId, const std::vector<StorageEdge>& edgesToAggregate, Graph* graph) const
{
	TRACE();

	struct EdgeInfo
	{
		Id edgeId;
		bool forward;
	};

	// build aggregation edges:
	// get all children of the active node
	std::vector<Id> childNodeIds = getAllChildNodeIds(nodeId);
	if (childNodeIds.size() == 0 && edgesToAggregate.size() == 0)
	{
		return;
	}

	// get all edges of the children
	std::map<Id, std::vector<EdgeInfo>> connectedNodeIds;
	for (const StorageEdge& edge : edgesToAggregate)
	{
		bool isSource = nodeId == edge.sourceNodeId;
		EdgeInfo edgeInfo;
		edgeInfo.edgeId = edge.id;
		edgeInfo.forward = isSource;
		connectedNodeIds[isSource ? edge.targetNodeId : edge.sourceNodeId].push_back(edgeInfo);
	}

	std::vector<StorageEdge> outgoingEdges = m_sqliteStorage.getEdgesBySourceIds(childNodeIds);
	for (const StorageEdge& outEdge : outgoingEdges)
	{
		EdgeInfo edgeInfo;
		edgeInfo.edgeId = outEdge.id;
		edgeInfo.forward = true;
		connectedNodeIds[outEdge.targetNodeId].push_back(edgeInfo);
	}

	std::vector<StorageEdge> incomingEdges = m_sqliteStorage.getEdgesByTargetIds(childNodeIds);
	for (const StorageEdge& inEdge : incomingEdges)
	{
		EdgeInfo edgeInfo;
		edgeInfo.edgeId = inEdge.id;
		edgeInfo.forward = false;
		connectedNodeIds[inEdge.sourceNodeId].push_back(edgeInfo);
	}

	// get all parent nodes of all connected nodes (up to last level except namespace/undefined)
	Id nodeParentNodeId = getLastVisibleParentNodeId(nodeId);

	std::map<Id, std::vector<EdgeInfo>> connectedParentNodeIds;
	for (const std::pair<Id, std::vector<EdgeInfo>>& p : connectedNodeIds)
	{
		Id parentNodeId = getLastVisibleParentNodeId(p.first);

		if (parentNodeId != nodeParentNodeId)
		{
			utility::append(connectedParentNodeIds[parentNodeId], p.second);
		}
	}

	// add hierarchies of these parents
	std::vector<Id> nodeIdsToAdd;
	for (const std::pair<Id, std::vector<EdgeInfo>> p : connectedParentNodeIds)
	{
		const Id aggregationTargetNodeId = p.first;
		if (!graph->getNodeById(aggregationTargetNodeId))
		{
			nodeIdsToAdd.push_back(aggregationTargetNodeId);
		}
	}
	addNodesWithChildrenAndEdgesToGraph(nodeIdsToAdd, std::vector<Id>(), graph);

	// create aggregation edges between parents and active node
	Node* sourceNode = graph->getNodeById(nodeId);
	for (const std::pair<Id, std::vector<EdgeInfo>> p : connectedParentNodeIds)
	{
		const Id aggregationTargetNodeId = p.first;

		Node* targetNode = graph->getNodeById(aggregationTargetNodeId);
		if (!targetNode)
		{
			LOG_ERROR("Aggregation target node not present.");
		}

		std::shared_ptr<TokenComponentAggregation> componentAggregation = std::make_shared<TokenComponentAggregation>();
		for (const EdgeInfo& edgeInfo: p.second)
		{
			componentAggregation->addAggregationId(edgeInfo.edgeId, edgeInfo.forward);
		}

		// Set first bit to 1 to avoid collisions
		Id aggregationId = ~(~size_t(0) >> 1) + *componentAggregation->getAggregationIds().begin();

		Edge* edge = graph->createEdge(aggregationId, Edge::EDGE_AGGREGATION, sourceNode, targetNode);
		edge->addComponentAggregation(componentAggregation);
	}
}

void PersistentStorage::addComponentAccessToGraph(Graph* graph) const
{
	TRACE();

	std::vector<Id> nodeIds;
	graph->forEachNode(
		[&nodeIds](Node* node)
		{
			nodeIds.push_back(node->getId());
		}
	);

	std::vector<StorageComponentAccess> accesses = m_sqliteStorage.getComponentAccessesByNodeIds(nodeIds);
	for (const StorageComponentAccess& access : accesses)
	{
		if (access.nodeId != 0)
		{
			graph->getNodeById(access.nodeId)->addComponentAccess(
				std::make_shared<TokenComponentAccess>(intToAccessKind(access.type)));
		}
	}
}

void PersistentStorage::buildSearchIndex()
{
	TRACE();

	FilePath dbPath = getDbFilePath();

	std::unordered_map<Id, StorageSymbol> symbolMap;
	for (StorageSymbol symbol : m_sqliteStorage.getAll<StorageSymbol>())
	{
		symbolMap[symbol.id] = symbol;
	}

	std::unordered_map<Id, StorageFile> fileMap;
	for (StorageFile file : m_sqliteStorage.getAll<StorageFile>())
	{
		fileMap[file.id] = file;
	}

	for (StorageNode node : m_sqliteStorage.getAll<StorageNode>())
	{
		if (Node::intToType(node.type) == Node::NODE_FILE)
		{
			auto it = fileMap.find(node.id);
			if (it != fileMap.end())
			{
				FilePath filePath = it->second.filePath;

				if (filePath.exists())
				{
					filePath = filePath.relativeTo(dbPath);
				}

				m_fileIndex.addNode(it->second.id, filePath.str());
			}
		}
		else
		{
			auto it = symbolMap.find(node.id);
			if (it == symbolMap.end() || intToDefinitionKind(it->second.definitionKind) != DEFINITION_IMPLICIT)
			{
				// we don't use the signature here, so elements with the same signature share the same node.
				m_symbolIndex.addNode(node.id, NameHierarchy::deserialize(node.serializedName).getQualifiedName());
			}
		}
	}

	m_symbolIndex.finishSetup();
	m_fileIndex.finishSetup();
}

void PersistentStorage::buildFilePathMaps()
{
	TRACE();

	for (StorageFile file: m_sqliteStorage.getAll<StorageFile>())
	{
		m_fileNodeIds.emplace(file.filePath, file.id);
		m_fileNodePaths.emplace(file.id, file.filePath);
	}
}

void PersistentStorage::buildFullTextSearchIndex() const
{
	TRACE();

	for (StorageFile file : m_sqliteStorage.getAll<StorageFile>())
	{
		m_fullTextSearchIndex.addFile(file.id, m_sqliteStorage.getFileContentById(file.id)->getText());
	}
}

void PersistentStorage::buildHierarchyCache()
{
	TRACE();

	std::vector<StorageEdge> memberEdges = m_sqliteStorage.getEdgesByType(Edge::typeToInt(Edge::EDGE_MEMBER));

	Cache<Id, Node::NodeType> nodeTypeCache([this](Id id){
		return Node::intToType(m_sqliteStorage.getFirstById<StorageNode>(id).type);
	});

	for (const StorageEdge& edge : memberEdges)
	{
		bool isVisible = !(nodeTypeCache.getValue(edge.sourceNodeId) & Node::NODE_NOT_VISIBLE);
		m_hierarchyCache.createConnection(edge.id, edge.sourceNodeId, edge.targetNodeId, isVisible);
	}
}

