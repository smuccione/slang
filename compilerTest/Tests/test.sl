function _keytodate(x)
    return x
end

class support
    method adesc ( a, b )
        return a + b;
    end
end

class cSkill 
    local aaCategory = "test2"
end

class test
    local aaStatus = "test"
    local support = new ( "support" )
    
    method userBadgeName( x )
        return x
    end

    method dataTemplate()
        local x
            a = {
                { 'rowClick', "doClick('#skillid#','#personid#');" },
                { 'data', {
                        { 'skillid' },
                        { 'personid' },
                        { 'alpha' },
                        { 'status' },
                        { 'category' },
                        { 'dateStart' },
                        { 'dateEnd' },
                        { 'dateUpdated' },
                        { 'lisc', "data.lisc" },
                        { 'certification', 'data.certification' },
                        { 'approvedby' }
                    }
                },
                { 'fields', {
                        { 'skillid', "{ editType:'hidden' }" },
                        { 'personid', "{ editType:'hidden' }" },
                        { 'alpha', "{ title:'Name' }" },
                        { 'status',, { |x| ::support.adesc( ::aaStatus, x ) } },
                        { 'category',, { |x| ::support.adesc( ::oskill.aaCategory, x ) } },
                        { 'dateStart', "{ title:'Date Issued' }", { |x| _keytodate(x) } },
                        { 'dateEnd', "{ title:'Date Expired' }", { |x| _keytodate(x) } },
                        { 'lisc' },
                        { 'certification' },
                        { 'approvedby', "{ title:'Assigned By' }", { |x| ::userBadgeName( x ) } }
                    }
                }
            }
    end

	ACCESS oskill
		static o = new( "cSkill" )
		return( o )
	END
end

